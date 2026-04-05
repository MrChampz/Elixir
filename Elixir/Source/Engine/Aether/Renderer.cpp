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

    void Renderer::Render(const std::vector<SRenderParticle>& renderables, const Camera& camera)
    {
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const std::size_t count = std::min(renderables.size(), MAX_PARTICLES);
        auto* vertices = static_cast<SVertex*>(m_VertexBuffer->Map());
        for (std::size_t i = 0; i < count; ++i)
        {
            vertices[i].Position = renderables[i].Position;
            vertices[i].Color = renderables[i].Color;
            vertices[i].Size = renderables[i].Size;
        }

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        BeginRendering(cmd);

        m_Pipeline->Bind(cmd);
        m_VertexBuffer->Bind(cmd);
        cmd->Draw(count);

        EndRendering(cmd);
    }

    void Renderer::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec2,  "Position" },
                    { EDataType::Vec4,  "Color"    },
                    { EDataType::Float, "Size"     }
                }
            }
        });

        m_Shader = shaderLoader->LoadShader("./Shaders/Aether/", "Particles");

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        builder.EnableAlphaBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_Pipeline = builder.Build(m_GraphicsContext);

        m_VertexBuffer = DynamicVertexBuffer::Create(m_GraphicsContext, sizeof(SVertex) * MAX_PARTICLES, m_Vertices.data());
        m_VertexBuffer->SetLayout(bufferLayout);
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
        m_Shader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
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
}