#include "epch.h"
#include "Renderer.h"

#include <Engine/Graphics/TextureLoader.h>
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

        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec2,  "Position"     },
                    { EDataType::Vec2,  "Size"         },
                    { EDataType::Float, "CornerRadius" },
                    { EDataType::Float, "Padding"      },
                    { EDataType::Vec4,  "Color"        },
                },
                EInputRate::Instance
            }
        });

        PipelineBuilder builder;
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
        builder.EnableAlphaBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);

        // Text rendering render entities

        m_Font = FontManager::Load("./Assets/Fonts/SF-Pro-Display-Regular.otf");
        //m_Font = FontManager::LoadPrecompiled("./Assets/Fonts/", "PlayfairDisplay-Regular");

        m_TextBatchContext.Shader = shaderLoader->LoadShader("./Shaders/", "Text");

        m_TextData.UnitRange = {
            m_Font->Atlas.Info.PxRange / m_Font->Atlas.Info.Width,
            m_Font->Atlas.Info.PxRange / m_Font->Atlas.Info.Height
        };

        SSamplerCreateInfo samplerInfo{};
        samplerInfo.MagFilter = ESamplerFilter::Linear;
        samplerInfo.MinFilter = ESamplerFilter::Linear;
        samplerInfo.MipmapMode = ESamplerMipmapMode::Linear;
        samplerInfo.AddressModeU = ESamplerAddressMode::ClampToEdge;
        samplerInfo.AddressModeV = ESamplerAddressMode::ClampToEdge;
        samplerInfo.AnisotropyEnabled = false;
        samplerInfo.MaxLod = 0.0f;
        auto sampler = Sampler::Create(m_GraphicsContext, samplerInfo);
        //m_Font->Atlas.Texture->SetSampler(sampler);

        m_TextBatchContext.Shader->BindTexture("hardmask", m_Font->Atlas.HardmaskTexture);
        m_TextBatchContext.Shader->BindTexture("mtsdf", m_Font->Atlas.MTSDFTexture);

        const BufferLayout bufferLayout1({
            {
                {
                    { EDataType::Vec2,  "Position" },
                    { EDataType::Vec2,  "TexCoord" }
                }
            }
        });

        builder.SetShader(m_TextBatchContext.Shader);
        builder.SetBufferLayout(bufferLayout1);

        m_TextBatchContext.Pipeline = builder.Build(context);

        m_TextBatchContext.Vertices.reserve(m_MaxVertices);
        m_TextBatchContext.VertexBuffer = DynamicVertexBuffer::Create(context, m_MaxVertices * sizeof(SVertex));
        m_TextBatchContext.VertexBuffer->SetLayout(bufferLayout1);

        m_TextBatchContext.Indices.reserve(m_MaxIndices);
        m_TextBatchContext.IndexBuffer = DynamicIndexBuffer::Create(context, m_MaxIndices * sizeof(uint32_t));

        // GUI rendering render entities
        m_Shader = shaderLoader->LoadShader("./Shaders/", "GUI");


        builder.SetShader(m_Shader);
        builder.SetBufferLayout(bufferLayout);
        m_Pipeline = builder.Build(context);

        m_Quads.reserve(m_MaxVertices);
        m_QuadBuffer = DynamicVertexBuffer::Create(context, m_MaxVertices * sizeof(SQuadData));
        m_QuadBuffer->SetLayout(bufferLayout);

        m_PerFrameConstantBuffer = UniformBuffer::Create(
            context,
            sizeof(SPerFrameData),
            &m_PerFrameData
        );
        m_TextBatchContext.Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);

        m_TextData.FontSize = 20.0f;
        m_TextConstantBuffer = UniformBuffer::Create(
            context,
            sizeof(STextData),
            &m_TextData
        );
        m_TextBatchContext.Shader->BindConstantBuffer("cbText", m_TextConstantBuffer);
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
        m_PerFrameConstantBuffer->UpdateData(&m_PerFrameData, sizeof(SPerFrameData));
    }

    void Renderer::Render(const RenderBatch& batch)
    {
        m_TextBatchContext.Clear();
        m_Quads.clear();

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

        if (!m_Quads.empty())
        {
            m_TextBatchContext.UpdateBuffers();
            m_QuadBuffer->UpdateData(m_Quads.data(),  m_Quads.size() * sizeof(SQuadData));

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
            m_QuadBuffer->Bind(cmd);
            cmd->Draw(6, m_Quads.size());

            m_TextBatchContext.Draw(cmd);

            cmd->EndRendering();
            m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
        }
    }

    void Renderer::BuildRectGeometry(const SDrawCommand& cmd)
    {
        SQuad quad;
        quad.Geometry = cmd.Geometry;
        quad.TexCoords = { {0, 0}, {1, 1} };
        quad.Color = cmd.Color;

        SQuadData data = {
            .Position = cmd.Geometry.Position,
            .Size = cmd.Geometry.Size,
            .CornerRadius = cmd.CornerRadius,
            .Color = cmd.Color
        };

        m_Quads.push_back(data);
    }

    void Renderer::BuildRectOutlineGeometry(const SDrawCommand& cmd)
    {

    }

    void Renderer::BuildTextGeometry(const SDrawCommand& cmd)
    {
        float cursorX = cmd.Geometry.Position.x;
        float cursorY = cmd.Geometry.Position.y;
        float scale = 1.0f / (m_Font->Atlas.Info.AscenderY - m_Font->Atlas.Info.DescenderY);

        for (const char c : cmd.Text)
        {
            auto glyph = m_Font->GetGlyph(c);

            if (glyph.has_value() && glyph.value().PlaneBounds.has_value())
            {
                SDrawCommand charCmd = cmd;

                const auto& planeBounds = glyph.value().PlaneBounds.value();

                float bearingX = planeBounds.Position.x;
                float bearingY = planeBounds.Position.y + planeBounds.Size.y;

                charCmd.Geometry.Position.x = cursorX + scale * bearingX * cmd.FontSize;
                charCmd.Geometry.Position.y = cursorY - scale * bearingY * cmd.FontSize;
                charCmd.Geometry.Size       = scale * planeBounds.Size * cmd.FontSize;

                const auto& atlasBounds = glyph.value().AtlasBounds.value();

                glm::vec2 texCoordMin = atlasBounds.Position;
                glm::vec2 texCoordMax = atlasBounds.Size;

                float texelWidth = 1.0f / m_Font->Atlas.Info.Width;
                float texelHeight = 1.0f / m_Font->Atlas.Info.Height;
                texCoordMin *= glm::vec2(texelWidth, texelHeight);
                texCoordMax *= glm::vec2(texelWidth, texelHeight);

                charCmd.TexCoords.Position = texCoordMin;
                charCmd.TexCoords.Size     = texCoordMax;

                charCmd.TexCoords.Position.y = 1.0f - charCmd.TexCoords.Position.y;
                charCmd.TexCoords.Size.y = 1.0f - charCmd.TexCoords.Size.y;

                BuildTextureGeometry(charCmd);
                cursorX += scale * glyph->Advance * cmd.FontSize;
            }
        }
    }

    void Renderer::BuildTextureGeometry(const SDrawCommand& cmd)
    {
        SQuad quad;
        quad.Geometry = cmd.Geometry;
        quad.TexCoords = cmd.TexCoords;
        quad.Color = cmd.Color;

        m_TextBatchContext.AddQuad(quad);
    }
}