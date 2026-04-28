#include "epch.h"
#include "Renderer.h"

#include "Engine/GUI/Renderer/QuadRenderPass.h"
#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir::Aether
{
    struct SSpawnPushConstants
    {
        uint32_t EmitterIndex = 0;
    };

    struct SRibbonPushConstants
    {
        uint32_t ParticleOffset = 0;
        uint32_t MaxParticles = 0;
        uint32_t HeadIndex = 0;
        float WidthScale = 1.0f;
    };

    SEmitterData ToEmitterDescription(const SGPUEmitter& emitter)
    {
        SEmitterData desc{};
        desc.MetaA = {
            emitter.ParticleOffset,
            emitter.MaxParticles,
            emitter.SpawnModuleOffset,
            emitter.SpawnModuleCount
        };

        desc.MetaB = {
            emitter.UpdateModuleOffset,
            emitter.UpdateModuleCount,
            0.0f,
            0.0f
        };

        desc.MetaC = {
            (float)emitter.RenderMode,
            emitter.SpawnRatePerSecond,
            emitter.GravityScale,
            0.0f
        };

        return desc;
    }

    SModuleData ToModuleDescription(const SGPUModule& module)
    {
        SModuleData desc{};

        desc.Header = {
            (float)(uint32_t)module.Type,
            (float)module.Target,
            module.Parameter0Index == UINT32_MAX ? -1.0f : module.Parameter0Index,
            module.Parameter1Index == UINT32_MAX ? -1.0f : module.Parameter1Index
        };

        desc.Data0 = module.Data0;
        desc.Data1 = module.Data1;

        return desc;
    }

    SParameterData ToParameterDescription(const SGPUParameter& parameter)
    {
        SParameterData desc{};
        desc.Value = parameter.Value;

        return desc;
    }

    Renderer::Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader)
        : m_GraphicsContext(context)
    {
        EE_CORE_INFO("Initializing Aether Renderer.")

        InitRingBuffer();
        InitRenderPass(shaderLoader);
        CreateBuffers();
        InitPerFrameData();
        BindShaderParameters();
    }

    void Renderer::Update(const Timestep& timestep)
    {
        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    void Renderer::Render(const SGPUSystem& system, const Camera& camera)
    {
        m_RenderExtent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const auto emitterCount = std::min((uint32_t)system.Emitters.size(), MAX_EMITTERS);
        const auto maxParticles = std::min(system.TotalMaxParticles, MAX_PARTICLES);

        UpdateParamsBuffer(system);

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        const auto* emitters = static_cast<const SEmitterData*>(m_EmitterBuffer->Map());

        m_SpawnPipeline->Bind(cmd);
        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto particleCount = (uint32_t)emitters[i].MetaA.y;
            if (particleCount == 0) continue;

            const SSpawnPushConstants pushConstants{ i };
            m_SpawnShader->SetPushConstant(cmd, "pc", (void*)&pushConstants, sizeof(SSpawnPushConstants));
            cmd->Dispatch((particleCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }

        m_ParticleBuffer->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);

        m_UpdatePipeline->Bind(cmd);
        cmd->Dispatch((maxParticles + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        m_ParticleBuffer->Barrier(cmd, EPipelineStage::VertexShader, EPipelineAccess::ShaderRead);

        BeginRendering(cmd);

        for (uint32_t i = 0; i < m_ActiveEmitters.size(); ++i)
        {
            const auto& emitter = m_ActiveEmitters[i];

            if (emitter.RenderMode == EParticleRenderMode::Ribbon)
            {
                if (emitter.MaxParticles < 2)
                    continue;

                const uint32_t headIndex = (m_SpawnCursors[i] + emitter.MaxParticles - 1u) % emitter.MaxParticles;

                const SRibbonPushConstants pushConstants
                {
                    emitter.ParticleOffset,
                    emitter.MaxParticles,
                    headIndex,
                    0.28f
                };

                m_RibbonShader->SetPushConstant(cmd, "pc", (void*)&pushConstants, sizeof(SRibbonPushConstants));
                m_RibbonPipeline->Bind(cmd);
                cmd->Draw((emitter.MaxParticles - 1u) * 6u);
                continue;
            }

            m_RendererPipeline->Bind(cmd);
            m_ParticleBuffer->BindAs<VertexBuffer>(cmd);
            cmd->Draw(emitter.MaxParticles, 1, emitter.ParticleOffset);
        }

        EndRendering(cmd);
    }

    void Renderer::InitRingBuffer()
    {
        m_SpawnAccumulators.assign(MAX_EMITTERS, 0.0f);
        m_SpawnCursors.assign(MAX_EMITTERS, 0u);
    }

    void Renderer::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec4,  "PositionSize" },
                    { EDataType::Vec4,  "VelocityAge"  },
                    { EDataType::Vec4,  "Color"        },
                    { EDataType::Vec4,  "Metadata"     }
                }
            }
        });

        m_SpawnShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSpawn" },
            "ParticlesSpawn",
            EShaderStage::Compute
        );

        m_UpdateShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesUpdate" },
            "ParticlesUpdate",
            EShaderStage::Compute
        );

        m_RendererShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Particles" },
            "ParticlesRenderer"
        );

        m_RibbonShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Ribbon" },
            "RibbonRenderer"
        );

        SPipelineCreateInfo pipelineInfo{};
        pipelineInfo.Shader = m_SpawnShader;
        m_SpawnPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_UpdateShader;
        m_UpdatePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        PipelineBuilder builder;
        builder.SetShader(m_RendererShader);
        builder.SetInputTopology(EPrimitiveTopology::PointList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        builder.EnableAlphaBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_RendererPipeline = builder.Build(m_GraphicsContext);

        builder.SetShader(m_RibbonShader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetBufferLayout({});
        m_RibbonPipeline = builder.Build(m_GraphicsContext);
    }

    void Renderer::CreateBuffers()
    {
        m_EmitterBuffer = DynamicStorageBuffer::Create(m_GraphicsContext, sizeof(SEmitterData) * MAX_EMITTERS);
        m_ModuleBuffer = DynamicStorageBuffer::Create(m_GraphicsContext, sizeof(SModuleData) * MAX_MODULES);
        m_ParameterBuffer = DynamicStorageBuffer::Create(m_GraphicsContext, sizeof(SParameterData) * MAX_PARAMETERS);
        m_ParticleBuffer = StorageBuffer::Create(m_GraphicsContext, sizeof(SGPUParticleState) * MAX_PARTICLES);
        m_ParamsBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(SParamsData));
    }

    void Renderer::InitPerFrameData()
    {
        m_FrameConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SFrameData),
            &m_FrameData
        );
    }

    void Renderer::BindShaderParameters() const
    {
        constexpr SSpawnPushConstants pushConstants{ 0 };
        m_SpawnShader->SetPushConstant("pc", (void*)&pushConstants, sizeof(SSpawnPushConstants));
        m_SpawnShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_SpawnShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_SpawnShader->BindStorageBuffer("modules", m_ModuleBuffer);
        m_SpawnShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_SpawnShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        m_UpdateShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_UpdateShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_UpdateShader->BindStorageBuffer("modules", m_ModuleBuffer);
        m_UpdateShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_UpdateShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        m_RendererShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

        constexpr SRibbonPushConstants ribbonPc{};
        m_RibbonShader->SetPushConstant("pc", (void*)&ribbonPc, sizeof(SRibbonPushConstants));
        m_RibbonShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_RibbonShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
        m_RibbonShader->BindConstantBuffer("cbParams", m_ParamsBuffer);
    }

    void Renderer::BeginRendering(const Ref<CommandBuffer>& cmd) const
    {
        const auto renderingInfo = SRenderingInfo
        {
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .RenderArea = m_RenderExtent
        };

        Viewport viewport = {};
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = m_RenderExtent.Width;
        viewport.Height = m_RenderExtent.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        Rect2D scissor = {};
        scissor.Offset = { 0, 0 };
        scissor.Extent = m_RenderExtent;

        cmd->BeginRendering(renderingInfo);
        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });
    }

    void Renderer::EndRendering(const Ref<CommandBuffer>& cmd) const
    {
        cmd->EndRendering();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
    }

    void Renderer::UpdateParamsBuffer(const SGPUSystem& system)
    {
        SParamsData params{};

        params.Time = {
            m_LastDeltaTimeSeconds,
            m_ElapsedTimeSeconds,
            (float)system.TotalMaxParticles,
            (float)system.Emitters.size(),
        };

        params.Viewport = {
            (float)m_RenderExtent.Width,
            (float)m_RenderExtent.Height,
            0.0f,
            0.0f
        };

        m_ParamsBuffer->UpdateData(&params, sizeof(SParamsData));

        const auto emitterCount = std::min(system.Emitters.size(), (size_t)MAX_EMITTERS);
        m_ActiveEmitters.assign(system.Emitters.begin(), system.Emitters.begin() + emitterCount);

        if (m_SpawnAccumulators.size() != emitterCount)
        {
            m_SpawnAccumulators.assign(emitterCount, 0.0f);
            m_SpawnCursors.assign(emitterCount, 0u);
        }

        auto* emitters = (SEmitterData*)m_EmitterBuffer->Map();
        for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
        {
            emitters[i] = {};
        }

        for (auto i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];
            m_SpawnAccumulators[i] += emitter.SpawnRatePerSecond * m_LastDeltaTimeSeconds;

            const uint32_t spawnCount = std::min((uint32_t)m_SpawnAccumulators[i], emitter.MaxParticles);
            if (spawnCount > 0)
                m_SpawnAccumulators[i] -= (float)spawnCount;

            auto desc = ToEmitterDescription(emitter);

            desc.MetaB.x = (float)emitter.UpdateModuleOffset;
            desc.MetaB.y = (float)emitter.UpdateModuleCount;
            desc.MetaB.z = (float)m_SpawnCursors[i];
            desc.MetaB.w = (float)spawnCount;

            emitters[i] = desc;

            if (emitter.MaxParticles > 0)
                m_SpawnCursors[i] = (m_SpawnCursors[i] + spawnCount) % emitter.MaxParticles;
        }

        auto* modules = (SModuleData*)m_ModuleBuffer->Map();
        for (size_t i = 0; i < system.Modules.size(); ++i)
        {
            modules[i] = ToModuleDescription(system.Modules[i]);
        }

        auto* parameters = (SParameterData*)m_ParameterBuffer->Map();
        for (size_t i = 0; i < system.Parameters.size(); ++i)
        {
            parameters[i] = ToParameterDescription(system.Parameters[i]);
        }
    }
}