#include "epch.h"
#include "Renderer.h"

#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/CommandBuffer.h>

namespace Elixir::GUI
{
    void Renderer::Initialize(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const Extent2D& extent
    )
    {
        EE_CORE_ASSERT(extent.Width > 0 && extent.Height > 0, "Render extent must be greater than zero!")
        EE_CORE_INFO("Initializing GUI Renderer [Width={}, Height={}].", extent.Width, extent.Height)
        m_GraphicsContext = context;
        m_RenderExtent = extent;
        m_PerFrameData.RenderExtent = { (float)extent.Width, (float)extent.Height };

        m_Shader = shaderLoader->LoadShader("./Shaders/", "GUI");

        const BufferLayout bufferLayout({
            { EDataType::Vec2, "Position" },
            { EDataType::Vec2, "TexCoord" },
            { EDataType::Vec4, "Color"    },
        });

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.DisableBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_Pipeline = builder.Build(context);

        m_Vertices.reserve(m_MaxVertices);
        m_VertexBuffer = DynamicVertexBuffer::Create(context, m_MaxVertices * sizeof(Vertex));
        m_VertexBuffer->SetLayout(bufferLayout);

        m_Indices.reserve(m_MaxIndices);
        m_IndexBuffer = DynamicIndexBuffer::Create(context, m_MaxIndices * sizeof(uint32_t));

        m_PerFrameConstantBuffer = UniformBuffer::Create(
            context,
            sizeof(PerFrameData),
            &m_PerFrameData
        );
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
    }

    void Renderer::Shutdown()
    {

    }

    void Renderer::Resize(const Extent2D& extent)
    {
        EE_CORE_ASSERT(extent.Width > 0 && extent.Height > 0, "Render extent must be greater than zero!")
        EE_CORE_INFO("Resizing GUI Renderer [Width={}, Height={}].", extent.Width, extent.Height)
        m_RenderExtent = extent;
        m_PerFrameData.RenderExtent = { (float)extent.Width, (float)extent.Height };
        m_PerFrameConstantBuffer->UpdateData(&m_PerFrameData, sizeof(PerFrameData));
    }

    void Renderer::Render(const RenderBatch& batch)
    {
        m_Vertices.clear();
        m_Indices.clear();

        for (const auto& drawCmd : batch.GetCommands())
        {
            switch (drawCmd.Type)
            {
                case SDrawCommand::EType::Rect:
                    BuildRectGeometry(drawCmd);
                    break;
                case SDrawCommand::EType::RectOutline:
                    BuildRectOutlineGeometry(drawCmd);
                    break;
                case SDrawCommand::EType::Text:
                    BuildTextGeometry(drawCmd);
                    break;
                case SDrawCommand::EType::Texture:
                    BuildTextureGeometry(drawCmd);
                    break;
                default:
                    EE_CORE_ERROR("Unknown SDrawCommand::EType!")
            }
        }

        if (!m_Vertices.empty())
        {
            m_VertexBuffer->UpdateData(m_Vertices.data(),  m_Vertices.size() * sizeof(Vertex));
            m_IndexBuffer->UpdateData(m_Indices.data(), m_Indices.size() * sizeof(uint32_t));

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

            const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
            cmd->BeginRendering(renderingInfo);
            cmd->SetViewports({ viewport });
            cmd->SetScissors({ scissor });
            m_Pipeline->Bind(cmd);
            m_VertexBuffer->Bind(cmd);
            m_IndexBuffer->Bind(cmd);
            cmd->DrawIndexed(m_Indices.size());
            cmd->EndRendering();
            m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
        }
    }

    void Renderer::BuildRectGeometry(const SDrawCommand& cmd)
    {
        const auto baseIndex = m_Vertices.size();

        // Create 4 vertices for the quad
        glm::vec2 topLeft = cmd.Geometry.Position;
        glm::vec2 bottomRight = cmd.Geometry.Position + cmd.Geometry.Size;

        m_Vertices.push_back({ topLeft, { 0, 0 }, cmd.Color });
        m_Vertices.push_back({ { bottomRight.x, topLeft.y }, { 1, 0 }, cmd.Color });
        m_Vertices.push_back({ bottomRight, { 1, 1 }, cmd.Color });
        m_Vertices.push_back({ { topLeft.x, bottomRight.y }, { 0, 1 }, cmd.Color });

        // Two triangles (6 indices)
        m_Indices.push_back(baseIndex + 0);
        m_Indices.push_back(baseIndex + 2);
        m_Indices.push_back(baseIndex + 1);
        m_Indices.push_back(baseIndex + 0);
        m_Indices.push_back(baseIndex + 3);
        m_Indices.push_back(baseIndex + 2);
    }

    void Renderer::BuildRectOutlineGeometry(const SDrawCommand& cmd)
    {

    }

    void Renderer::BuildTextGeometry(const SDrawCommand& cmd)
    {

    }

    void Renderer::BuildTextureGeometry(const SDrawCommand& cmd)
    {

    }
}