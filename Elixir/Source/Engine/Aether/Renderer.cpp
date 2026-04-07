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

    void Renderer::Render(const SGPUSystem& system, const Camera& camera)
    {
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const auto maxParticles = std::min(system.Emitter.MaxParticles, MAX_PARTICLES);

        UpdateParamsBuffer(system);

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
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

        m_ParticleBuffer = VertexBuffer::Create(m_GraphicsContext, sizeof(SGPUParticleVertex) * MAX_PARTICLES);
        m_ParticleBuffer->SetLayout(bufferLayout);
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

    }
}