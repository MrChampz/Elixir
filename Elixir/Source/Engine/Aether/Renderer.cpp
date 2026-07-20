#include "epch.h"
#include "Renderer.h"

#include "Engine/Core/Color.h"
#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/SamplerBuilder.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir::Aether
{
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    struct SSpritePushConstants
    {
        uint32_t SpriteIndex = 0;
    };

    struct SRibbonPushConstants
    {
        uint32_t EmitterIndex = 0;
    };

    SEmitterData ToEmitterDescription(const SCompiledEmitter& emitter)
    {
        SEmitterData desc{};
        desc.MetaA = {
            emitter.LocalParticleOffset,
            emitter.MaxParticles,
            emitter.SpawnOpOffset,
            emitter.SpawnOpCount
        };

        desc.MetaB = {
            emitter.UpdateOpOffset,
            emitter.UpdateOpCount,
            emitter.TriggerTargetOffset,
            emitter.TriggerTargetCount
        };

        desc.MetaC = {
            (float)emitter.RenderMode,
            emitter.SpawnRatePerSecond,
            emitter.GravityScale,
            emitter.BurstIntervalSeconds
        };

        desc.MetaD = {
            (float)emitter.BurstCount,
            emitter.IsTriggerDriven ? 1.0f : 0.0f,
            0.0f,
            0.0f
        };

        return desc;
    }

    STriggerTargetData ToTriggerTargetDescription(const SCompiledTriggerTarget& target)
    {
        return {
            .TargetEmitterIndex = target.TargetEmitterIndex,
            .BurstCount = target.BurstCount,
            .DelaySeconds = target.DelaySeconds,
        };
    }

    SParticleOpData ToOpDescription(const SGPUParticleOp& op)
    {
        SParticleOpData desc{};

        desc.Header = {
            (float)(uint32_t)op.Type,
            (float)op.Target,
            op.Parameter0Index == UINT32_MAX ? -1.0f : (float)op.Parameter0Index,
            op.Parameter1Index == UINT32_MAX ? -1.0f : (float)op.Parameter1Index
        };

        desc.Data0 = op.Data0;
        desc.Data1 = op.Data1;
        desc.Data2 = op.Data2;

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

        Init(shaderLoader);
        CreateBuffers();
        InitPerFrameData();
        BindShaderParameters();
    }

    void Renderer::Update(const Timestep& timestep)
    {
        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    void Renderer::Render(const SCompiledSystem& system, const Camera& camera)
    {
        m_LastSubmissionMetrics = {
            .SubmissionSerial = ++m_SubmissionSerial,
            .DeltaTimeSeconds = m_LastDeltaTimeSeconds,
            .ElapsedTimeSeconds = m_ElapsedTimeSeconds,
            .RequestedEmitterCount =  system.Emitters.size(),
            .RequestedParticleCapacity = system.TotalMaxParticles
        };

        m_RenderExtent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        m_FrameData.View = camera.GetViewMatrix();
        m_FrameData.Proj = camera.GetProjectionMatrix();
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameData.CameraPos = camera.GetPosition();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const auto emitterCount = std::min((uint32_t)system.Emitters.size(), MAX_EMITTERS);
        const auto maxParticles = std::min(system.TotalMaxParticles, MAX_PARTICLES);

        m_LastSubmissionMetrics.SubmittedEmitterCount = emitterCount;
        m_LastSubmissionMetrics.SubmittedParticleCapacity = maxParticles;
        m_LastSubmissionMetrics.ScheduledEmitterCount = emitterCount;
        m_LastSubmissionMetrics.TriggerEventCapacityPerEmitter =
            MAX_TRIGGER_EVENTS_PER_EMITTER;

        UpdateBuffers(system);

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        const auto* emitters = static_cast<const SEmitterData*>(m_EmitterBuffer->Map());

        // Scheduling

        constexpr SSchedulePushConstants schedulePushConstants{ .InstanceIndex = 0 };

        m_ScheduleBeginPipeline->Bind(cmd);
        m_ScheduleBeginShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch((emitterCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        BarrierSchedulingBuffers(cmd);

        m_ScheduleEmittersPipeline->Bind(cmd);
        m_ScheduleEmittersShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch((emitterCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        BarrierSchedulingBuffers(cmd);

        m_ScheduleFinalizePipeline->Bind(cmd);
        m_ScheduleFinalizeShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch(1);

        BarrierSchedulingBuffers(cmd);

        // Spawning

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        m_SpawnPipeline->Bind(cmd);
        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto offset = (uint32_t)emitters[i].MetaA.x;
            if (offset >= MAX_PARTICLES) continue;

            const auto particleCount = std::min((uint32_t)emitters[i].MetaA.y, MAX_PARTICLES - offset);
            if (particleCount == 0) continue;

            ++m_LastSubmissionMetrics.SpawnDispatchCount;

            const SSpawnPushConstants spawnPushConstants
            {
                .InstanceIndex = 0,
                .EmitterIndex = i,
            };

            m_SpawnShader->SetPushConstant(
                cmd,
                "pc",
                (void*)&spawnPushConstants,
                sizeof(SSpawnPushConstants)
            );

            cmd->Dispatch((particleCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }

        // Updating

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        m_UpdatePipeline->Bind(cmd);

        constexpr SUpdatePushConstants updatePushConstants
        {
            .InstanceIndex = 0,
        };

        m_UpdateShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&updatePushConstants,
            sizeof(SUpdatePushConstants)
        );

        cmd->Dispatch((maxParticles + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        // Rendering

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::VertexShader | EPipelineStage::VertexInput,
            EPipelineAccess::ShaderRead | EPipelineAccess::VertexAttributeRead
        );

        BeginRendering(cmd);

        bool spritePipelineBound = false;
        bool ribbonPipelineBound = false;

        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];

            if (emitter.RenderMode != EParticleRenderMode::Mesh || emitter.MaxParticles == 0)
                continue;

            m_MeshPipeline->Bind(cmd);
            m_MeshVertexBuffer->Bind(cmd);
            // TODO: Enhance this api
            m_ParticleBuffer->BindAs<VertexBuffer>(cmd, std::span<uint64_t>{}, 1, 1);

            cmd->Draw(
                m_MeshVertexCount,
                emitter.MaxParticles,
                0,
                emitter.LocalParticleOffset
            );
        }

        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];

            if (emitter.RenderMode == EParticleRenderMode::Mesh || emitter.MaxParticles == 0)
                continue;

            if (emitter.RenderMode == EParticleRenderMode::Ribbon)
            {
                if (!ribbonPipelineBound)
                {
                    m_RibbonPipeline->Bind(cmd);
                    ribbonPipelineBound = true;
                    spritePipelineBound = false;
                }

                const SRibbonPushConstants pc{ i };
                m_RibbonShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(SRibbonPushConstants));

                cmd->Draw(emitter.MaxParticles * 6);
                continue;
            }

            if (!spritePipelineBound)
            {
                m_SpritePipeline->Bind(cmd);
                m_ParticleBuffer->BindAs<VertexBuffer>(cmd);
                spritePipelineBound = true;
                ribbonPipelineBound = false;
            }

            const SSpritePushConstants pc{ ResolveSpriteIndex(emitter.SpriteTexture) };
            m_SpriteShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(SSpritePushConstants));

            cmd->Draw(6, emitter.MaxParticles, 0, emitter.LocalParticleOffset);
        }

        EndRendering(cmd);
    }

    const SParticleSubmissionMetrics& Renderer::GetLastSubmissionMetrics() const
    {
        return m_LastSubmissionMetrics;
    }

    void Renderer::Init(const ShaderLoader* shaderLoader)
    {
        m_ScheduleBeginShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesScheduleBegin" },
            "ParticlesScheduleBegin",
            EShaderStage::Compute
        );

        m_ScheduleEmittersShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesScheduleEmitters" },
            "ParticlesScheduleEmitters",
            EShaderStage::Compute
        );

        m_ScheduleFinalizeShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesScheduleFinalize" },
            "ParticlesScheduleFinalize",
            EShaderStage::Compute
        );

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

        m_SpriteShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Sprite" },
            "SpriteRenderer"
        );

        m_RibbonShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Ribbon" },
            "RibbonRenderer"
        );

        m_MeshShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Mesh" },
            "MeshRenderer"
        );

        SPipelineCreateInfo pipelineInfo{};
        pipelineInfo.Shader = m_ScheduleBeginShader;
        m_ScheduleBeginPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_ScheduleEmittersShader;
        m_ScheduleEmittersPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_ScheduleFinalizeShader;
        m_ScheduleFinalizePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_SpawnShader;
        m_SpawnPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_UpdateShader;
        m_UpdatePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        const BufferLayout spriteBufferLayout({
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        });

        PipelineBuilder spriteBuilder;
        spriteBuilder.SetShader(m_SpriteShader);
        spriteBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        spriteBuilder.SetPolygonMode(EPolygonMode::Fill);
        spriteBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        spriteBuilder.EnableAlphaBlending();
        spriteBuilder.DisableDepthTest();
        spriteBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        spriteBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        spriteBuilder.SetBufferLayout(spriteBufferLayout);
        m_SpritePipeline = spriteBuilder.Build(m_GraphicsContext);

        m_Sprites = TextureSet::Create(m_GraphicsContext);
        m_SpriteSampler = SamplerBuilder().Build(m_GraphicsContext);

        PipelineBuilder ribbonBuilder;
        ribbonBuilder.SetShader(m_RibbonShader);
        ribbonBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        ribbonBuilder.SetPolygonMode(EPolygonMode::Fill);
        ribbonBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        ribbonBuilder.EnableAlphaBlendingMax();
        ribbonBuilder.DisableDepthTest();
        ribbonBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        ribbonBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        ribbonBuilder.SetBufferLayout({});
        m_RibbonPipeline = ribbonBuilder.Build(m_GraphicsContext);

        const BufferLayout meshBufferLayout({
            {
                {
                    { EDataType::Vec3,  "Position" },
                    { EDataType::Vec3,  "Normal"   },
                },
                EInputRate::Vertex
            },
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        });

        PipelineBuilder meshBuilder;
        meshBuilder.SetShader(m_MeshShader);
        meshBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        meshBuilder.SetPolygonMode(EPolygonMode::Fill);
        meshBuilder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
        meshBuilder.EnableAlphaBlendingMax();
        meshBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        meshBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        meshBuilder.SetBufferLayout(meshBufferLayout);

        auto info = meshBuilder.GetCreateInfo();
        info.DepthStencil.DepthTestEnable = true;
        info.DepthStencil.DepthWriteEnable = true;
        info.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;

        m_MeshPipeline = GraphicsPipeline::Create(m_GraphicsContext, info);
    }

    void Renderer::CreateBuffers()
    {
        m_ParticleBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUParticleState) * MAX_PARTICLES
        );

        m_EmitterStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SEmitterInstanceStateData) * MAX_SYSTEM_INSTANCES * MAX_EMITTERS
        );

        m_SpawnRequestBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SSpawnRequestData) * MAX_SYSTEM_INSTANCES * MAX_EMITTERS
        );

        m_TriggerTargetBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(STriggerTargetData) * MAX_TRIGGER_TARGETS
        );

        for (auto& buffer : m_TriggerEventBuffers)
        {
            buffer = StorageBuffer::Create(
                m_GraphicsContext,
                sizeof(STriggerEventData) *
                    MAX_SYSTEM_INSTANCES *
                    MAX_EMITTERS *
                    MAX_TRIGGER_EVENTS_PER_EMITTER
            );
            buffer->Clear();
        }

        m_TriggerQueueStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(STriggerTargetData) * MAX_TRIGGER_TARGETS
        );

        m_SystemInstanceBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(STriggerQueueStateData) * MAX_SYSTEM_INSTANCES * MAX_EMITTERS * 2
        );

        m_SystemSchedulerStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SSystemSchedulerStateData) * MAX_SYSTEM_INSTANCES
        );

        m_EmitterStateBuffer->Clear();
        m_SpawnRequestBuffer->Clear();
        m_TriggerQueueStateBuffer->Clear();
        m_SystemSchedulerStateBuffer->Clear();

        m_EmitterBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SEmitterData) * MAX_EMITTERS
        );

        m_OpBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SParticleOpData) * MAX_OPS
        );

        m_ParameterBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SParameterData) * MAX_PARAMETERS
        );

        m_ParamsBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SParamsData)
        );

        CreateMeshVertexBuffer();
    }

    void Renderer::CreateMeshVertexBuffer()
    {
        static constexpr std::array<MeshVertex, 36> vertices = {{
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},

            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},

            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
        }};

        m_MeshVertexCount = (uint32_t)vertices.size();
        m_MeshVertexBuffer = VertexBuffer::Create(
            m_GraphicsContext,
            sizeof(MeshVertex) * vertices.size(),
            vertices.data()
        );

        m_MeshVertexBuffer->SetLayout(m_MeshPipeline->GetBufferLayout());
    }

    void Renderer::InitPerFrameData()
    {
        m_FrameConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SFrameData),
            &m_FrameData
        );
    }

    void Renderer::BindShaderParameters()
    {
        constexpr SSchedulePushConstants schedulePushConstants{ .InstanceIndex = 0 };

        m_ScheduleBeginShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_ScheduleBeginShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_ScheduleBeginShader->BindStorageBuffer("emitterStates", m_EmitterStateBuffer);
        m_ScheduleBeginShader->BindStorageBuffer("triggerQueueStates", m_TriggerQueueStateBuffer);
        m_ScheduleBeginShader->BindStorageBuffer("schedulerStates", m_SystemSchedulerStateBuffer);

        m_ScheduleEmittersShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_ScheduleEmittersShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_ScheduleEmittersShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_ScheduleEmittersShader->BindStorageBuffer("emitterStates", m_EmitterStateBuffer);
        m_ScheduleEmittersShader->BindStorageBuffer("spawnRequests", m_SpawnRequestBuffer);
        m_ScheduleEmittersShader->BindStorageBuffer("triggerTargets", m_TriggerTargetBuffer);
        m_ScheduleEmittersShader->BindStorageBuffer("triggerEventsA", m_TriggerEventBuffers[0]);
        m_ScheduleEmittersShader->BindStorageBuffer("triggerEventsB", m_TriggerEventBuffers[1]);
        m_ScheduleEmittersShader->BindStorageBuffer("triggerQueueStates", m_TriggerQueueStateBuffer);
        m_ScheduleEmittersShader->BindStorageBuffer("schedulerStates", m_SystemSchedulerStateBuffer);
        m_ScheduleEmittersShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        m_ScheduleFinalizeShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_ScheduleFinalizeShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_ScheduleFinalizeShader->BindStorageBuffer("schedulerStates", m_SystemSchedulerStateBuffer);

        constexpr SSpawnPushConstants spawnPushConstants
        {
            .InstanceIndex = 0,
            .EmitterIndex = 0
        };

        m_SpawnShader->SetPushConstant(
            "pc",
            (void*)&spawnPushConstants,
            sizeof(spawnPushConstants)
        );

        m_SpawnShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_SpawnShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_SpawnShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_SpawnShader->BindStorageBuffer("spawnRequests", m_SpawnRequestBuffer);
        m_SpawnShader->BindStorageBuffer("ops", m_OpBuffer);
        m_SpawnShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_SpawnShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        constexpr SUpdatePushConstants updatePushConstants{ .InstanceIndex = 0 };
        m_UpdateShader->SetPushConstant(
            "pc",
            (void*)&updatePushConstants,
            sizeof(updatePushConstants)
        );

        m_UpdateShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_UpdateShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_UpdateShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_UpdateShader->BindStorageBuffer("ops", m_OpBuffer);
        m_UpdateShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_UpdateShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        const auto whiteTex = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_SRGB,
            1, 1,
            &Color::WhiteAlpha
        );

        m_WhiteTextureHandle = m_Sprites->AddTexture(whiteTex);

        const SSpritePushConstants spritePushConstants{ m_WhiteTextureHandle.Index };
        m_SpriteShader->SetPushConstant(
            "pc",
            (void*)&spritePushConstants,
            sizeof(spritePushConstants)
        );

        m_SpriteShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
        m_SpriteShader->BindTextureSet("sprites", m_Sprites);
        m_SpriteShader->BindSampler("spriteSampler", m_SpriteSampler);

        constexpr SRibbonPushConstants ribbonPushConstants{ 0 };
        m_RibbonShader->SetPushConstant(
            "pc",
            (void*)&ribbonPushConstants,
            sizeof(ribbonPushConstants)
        );

        m_RibbonShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_RibbonShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_RibbonShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

        m_MeshShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
    }

    uint32_t Renderer::ResolveSpriteIndex(const Ref<Texture2D>& texture)
    {
        if (!texture)
            return m_WhiteTextureHandle.Index;

        if (const auto it = m_SpriteTextures.find(texture); it != m_SpriteTextures.end())
            return it->second.Index;

        const auto handle = m_Sprites->AddTexture(texture);
        m_SpriteTextures[texture] = handle;

        return handle.Index;
    }

    void Renderer::BeginRendering(const Ref<CommandBuffer>& cmd) const
    {
        const auto renderingInfo = SRenderingInfo
        {
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        };

        Viewport viewport = {};
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = (float)m_RenderExtent.Width;
        viewport.Height = (float)m_RenderExtent.Height;
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

    void Renderer::UpdateBuffers(const SCompiledSystem& system)
    {
        // The GPU buffers are fixed-capacity. If the built system exceeds any of
        // them, the copy below clamps silently while emitter op offsets/counts
        // (computed without these limits in Emitter::Build) keep pointing past
        // the uploaded range — i.e. out-of-bounds reads on the GPU. Surface it
        // loudly once instead of corrupting the dispatch.
        if (!m_CapacityErrorReported &&
            (system.Emitters.size()   > MAX_EMITTERS ||
             system.Ops.size()        > MAX_OPS      ||
             system.Parameters.size() > MAX_PARAMETERS ||
             system.TriggerTargets.size() > MAX_TRIGGER_TARGETS))
        {

            EE_CORE_ERROR(
                "Aether system '{}' exceeds renderer capacity and will be truncated "
                "(emitters {}/{}, ops {}/{}, parameters {}/{}, trigger targets {}/{}).",
                system.Name,
                system.Emitters.size(),   MAX_EMITTERS,
                system.Ops.size(),        MAX_OPS,
                system.Parameters.size(), MAX_PARAMETERS,
                system.TriggerTargets.size(), MAX_TRIGGER_TARGETS
            )

            m_CapacityErrorReported = true;
        }

        SParamsData params{
            .Time = { m_LastDeltaTimeSeconds, m_ElapsedTimeSeconds, 0.0f, 0.0f },
            .Viewport = {
                (float)m_RenderExtent.Width,
                (float)m_RenderExtent.Height,
                0.0f,
                0.0f
            }
        };
        m_ParamsBuffer->UpdateData(&params, sizeof(SParamsData));

        if (!m_HasBoundCompiledSystem ||
            m_BoundCompiledSystemId != system.SourceId ||
            m_BoundCompilationRevision != system.CompilationRevision)
        {
            m_HasBoundCompiledSystem = true;
            m_BoundCompiledSystemId = system.SourceId;
            m_BoundCompilationRevision = system.CompilationRevision;
            ++m_SystemInstanceGeneration;
        }

        const SSystemInstanceData instanceData
        {
            .ParticleBaseOffset = 0,
            .EmitterBaseOffset = 0,
            .OpBaseOffset = 0,
            .ParameterBaseOffset = 0,
            .EmitterStateBaseOffset = 0,
            .SpawnRequestBaseOffset = 0,
            .TriggerEventBaseOffset = 0,
            .TriggerQueueStateBaseOffset = 0,
            .ParticleCount = std::min(system.TotalMaxParticles, MAX_PARTICLES),
            .EmitterCount = (uint32_t)std::min(system.Emitters.size(), (size_t)MAX_EMITTERS),
            .TriggerEventCapacityPerEmitter = MAX_TRIGGER_EVENTS_PER_EMITTER,
            .Generation = m_SystemInstanceGeneration,
            .ParticleStateLayoutIndex = (uint32_t)system.ParticleStateLayout,
        };

        m_SystemInstanceBuffer->UpdateData(&instanceData, sizeof(SSystemInstanceData));

        auto* emitters = (SEmitterData*)m_EmitterBuffer->Map();
        const auto emitterCount = std::min(
            system.Emitters.size(),
            (size_t)MAX_EMITTERS
        );
        for (size_t i = 0; i < emitterCount; ++i)
            emitters[i] = ToEmitterDescription(system.Emitters[i]);
        for (size_t i = emitterCount; i < MAX_EMITTERS; ++i)
            emitters[i] = {};

        auto* ops = (SParticleOpData*)m_OpBuffer->Map();
        const size_t opCount = std::min(system.Ops.size(), (size_t)MAX_OPS);
        for (size_t i = 0; i < opCount; ++i)
            ops[i] = ToOpDescription(system.Ops[i]);
        for (size_t i = opCount; i < MAX_OPS; ++i)
            ops[i] = {};

        auto* parameters = (SParameterData*)m_ParameterBuffer->Map();
        const size_t parameterCount = std::min(
            system.Parameters.size(),
            (size_t)MAX_PARAMETERS
        );
        for (size_t i = 0; i < parameterCount; ++i)
            parameters[i] = ToParameterDescription(system.Parameters[i]);
        for (size_t i = parameterCount; i < MAX_PARAMETERS; ++i)
            parameters[i] = {};

        auto* targets = (STriggerTargetData*)m_TriggerTargetBuffer->Map();
        const auto targetCount = std::min(
            system.TriggerTargets.size(),
            (size_t)MAX_TRIGGER_TARGETS
        );
        for (size_t i = 0; i < targetCount; ++i)
            targets[i] = ToTriggerTargetDescription(system.TriggerTargets[i]);
        for (size_t i = targetCount; i < MAX_TRIGGER_TARGETS; ++i)
            targets[i] = {};
    }

    void Renderer::BarrierSchedulingBuffers(const Ref<CommandBuffer>& cmd) const
    {
        constexpr auto stage = EPipelineStage::ComputeShader;
        constexpr auto access = EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite;

        m_EmitterStateBuffer->Barrier(cmd, stage, access);
        m_SpawnRequestBuffer->Barrier(cmd, stage, access);

        for (auto& buffer : m_TriggerEventBuffers)
            buffer->Barrier(cmd, stage, access);

        m_TriggerQueueStateBuffer->Barrier(cmd, stage, access);
        m_SystemSchedulerStateBuffer->Barrier(cmd, stage, access);
    }
}
