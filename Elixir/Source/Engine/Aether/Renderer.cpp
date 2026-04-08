#include "epch.h"
#include "Renderer.h"

#include "Engine/GUI/Renderer/QuadRenderPass.h"
#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir::Aether
{
    Renderer::Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader)
        : m_GraphicsContext(context)
    {
        EE_CORE_INFO("Initializing Aether Renderer.")

        m_RenderExtent = { 1920, 1080 };

        InitRenderPass(shaderLoader);
        InitPerFrameData();
        BindShaderParameters();
    }

    void Renderer::Update(const Timestep& timestep)
    {
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    void Renderer::Render(const SGPUSystem& system, const Camera& camera)
    {
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const auto maxParticles = std::min(system.Emitter.MaxParticles, MAX_PARTICLES);

        UpdateParamsBuffer(system);

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        m_SimulationPipeline->Bind(cmd);
        cmd->Dispatch((maxParticles + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        m_ParticleBuffer->Barrier(cmd, EPipelineStage::VertexShader, EPipelineAccess::ShaderRead);

        BeginRendering(cmd);

        m_RendererPipeline->Bind(cmd);
        m_ParticleBuffer->Bind(cmd);
        cmd->Draw(maxParticles);

        EndRendering(cmd);
    }

    void Renderer::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec4,  "PositionSize" },
                    { EDataType::Vec4,  "Color"        }
                }
            }
        });

        m_SimulationShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Particles" },
            "ParticlesSimulation",
            EShaderStage::Compute
        );

        m_RendererShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Particles" },
            "ParticlesRenderer"
        );

        SPipelineCreateInfo simulationPipelineInfo{};
        simulationPipelineInfo.Shader = m_SimulationShader;
        m_SimulationPipeline = ComputePipeline::Create(m_GraphicsContext, simulationPipelineInfo);

        PipelineBuilder builder;
        builder.SetShader(m_RendererShader);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        builder.EnableAlphaBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_RendererPipeline = builder.Build(m_GraphicsContext);

        m_ParticleBuffer = StorageBuffer::Create(m_GraphicsContext, sizeof(SGPUParticleVertex) * MAX_PARTICLES);
        m_ParticleBuffer->SetLayout(bufferLayout);

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
        m_SimulationShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_SimulationShader->BindConstantBuffer("cbParams", m_ParamsBuffer);
        m_RendererShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
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

        const auto& emitter = system.Emitter;

        params.SpawnCenterRadiusRate = { emitter.SpawnCenter.x, emitter.SpawnCenter.y, emitter.SpawnRadius, emitter.SpawnRatePerSecond };
        params.AngleSpeed = { emitter.AngleMinRadians, emitter.AngleMaxRadians, emitter.SpeedMin, emitter.SpeedMax };
        params.LifetimeSize = { emitter.LifetimeMin, emitter.LifetimeMax, emitter.SizeStart, emitter.SizeEnd };
        params.GravityDragTime = { emitter.Gravity.x, emitter.Gravity.y, emitter.Drag, m_ElapsedTimeSeconds };
        params.BoundsMin = { emitter.MinBounds.x, emitter.MinBounds.y, 0.0f, 0.0f };
        params.BoundsMax = { emitter.MaxBounds.x, emitter.MaxBounds.y, 0.0f, 0.0f };
        params.ColorStart = emitter.ColorStart;
        params.ColorEnd = emitter.ColorEnd;
        params.Meta = { (float)emitter.MaxParticles, emitter.GravityScale, 0.0f, 0.0f };

        m_ParamsBuffer->UpdateData(&params, sizeof(SParamsData));
    }
}