#include "epch.h"
#include "TextRenderPass.h"

#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/SamplerBuilder.h>

namespace Elixir::GUI
{
    TextRenderPass::TextRenderPass(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const Ref<UniformBuffer>& perFrameCB
    ) : m_PerFrameConstantBuffer(perFrameCB), m_GraphicsContext(context)
    {
        EE_CORE_TRACE("Initializing GUI: TextRenderPass.")
        InitFontData();
        InitRenderPass(shaderLoader);
        BindShaderParameters();
    }

    void TextRenderPass::GenerateDrawCommands(const RenderBatch& batch)
    {
        m_Vertices.clear();
        m_Indices.clear();

        for (const auto& drawCmd : batch.GetCommands())
        {
            switch (drawCmd.Type)
            {
                case SDrawCommand::EType::Text:
                    BuildTextGeometry(drawCmd);
                    break;
                default:
                    break;
            }
        }

        if (!m_Vertices.empty())
        {
            m_VertexBuffer->UpdateData(m_Vertices.data(),  m_Vertices.size() * sizeof(SVertex));
            m_IndexBuffer->UpdateData(m_Indices.data(),  m_Indices.size() * sizeof(uint32_t));
        }
    }

    void TextRenderPass::Render(const Ref<CommandBuffer>& cmd)
    {
        m_Pipeline->Bind(cmd);
        m_VertexBuffer->Bind(cmd);
        m_IndexBuffer->Bind(cmd);
        cmd->DrawIndexed(m_Indices.size());
    }

    bool TextRenderPass::HasData() const
    {
        return !m_Vertices.empty();
    }

    void TextRenderPass::Clear()
    {
        m_Vertices.clear();
        m_Indices.clear();
    }

    void TextRenderPass::InitFontData()
    {
        m_Font = FontManager::Load("./Assets/Fonts/SF-Pro-Display-Regular.otf");
        //m_Font = FontManager::LoadPrecompiled("./Assets/Fonts/", "PlayfairDisplay-Regular");

        m_FontData.FontSize = 20.0f;
        m_FontData.UnitRange = {
            m_Font->Atlas.Info.PxRange / m_Font->Atlas.Info.Width,
            m_Font->Atlas.Info.PxRange / m_Font->Atlas.Info.Height
        };
    }

    void TextRenderPass::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec2,  "Position" },
                    { EDataType::Vec2,  "TexCoord" }
                }
            }
        });

        m_Shader = shaderLoader->LoadShader("./Shaders/", "Text");

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
        builder.EnableAlphaBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_Pipeline = builder.Build(m_GraphicsContext);

        m_Vertices.reserve(MAX_VERTICES);
        m_VertexBuffer = DynamicVertexBuffer::Create(m_GraphicsContext, MAX_VERTICES * sizeof(SVertex));
        m_VertexBuffer->SetLayout(bufferLayout);

        m_Indices.reserve(MAX_INDICES);
        m_IndexBuffer = DynamicIndexBuffer::Create(m_GraphicsContext, MAX_INDICES * sizeof(uint32_t));

        m_FontConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SFontData),
            &m_FontData
        );
    }

    void TextRenderPass::BindShaderParameters() const
    {
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
        m_Shader->BindConstantBuffer("cbFont", m_FontConstantBuffer);
        m_Shader->BindTexture("hardmask", m_Font->Atlas.HardmaskTexture);
        m_Shader->BindTexture("mtsdf", m_Font->Atlas.MTSDFTexture);

        const auto sampler = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Linear)
            .SetMinFilter(ESamplerFilter::Linear)
            .Build(m_GraphicsContext);
        m_Shader->BindSampler("atlasSampler", sampler);
    }

    void TextRenderPass::BuildTextGeometry(const SDrawCommand& cmd)
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

    void TextRenderPass::BuildTextureGeometry(const SDrawCommand& cmd)
    {
        const auto baseIndex = m_Vertices.size();

        // Create 4 vertices for the quad
        const auto topLeft = cmd.Geometry.Position;
        const auto bottomRight = cmd.Geometry.Position + cmd.Geometry.Size;
        const auto topRight = glm::vec2{ bottomRight.x, topLeft.y };
        const auto bottomLeft = glm::vec2{ topLeft.x, bottomRight.y };

        const auto topLeftUV = glm::vec2{ cmd.TexCoords.Position.x, cmd.TexCoords.Size.y };
        const auto bottomRightUV = glm::vec2{ cmd.TexCoords.Size.x, cmd.TexCoords.Position.y };
        const auto topRightUV = glm::vec2{ cmd.TexCoords.Size.x, cmd.TexCoords.Size.y };
        const auto bottomLeftUV = glm::vec2{ cmd.TexCoords.Position.x, cmd.TexCoords.Position.y };

        m_Vertices.push_back({ topLeft,     topLeftUV     });
        m_Vertices.push_back({ topRight,    topRightUV    });
        m_Vertices.push_back({ bottomRight, bottomRightUV });
        m_Vertices.push_back({ bottomLeft,  bottomLeftUV  });

        // Two triangles (6 indices)
        m_Indices.push_back(baseIndex + 0);
        m_Indices.push_back(baseIndex + 2);
        m_Indices.push_back(baseIndex + 1);
        m_Indices.push_back(baseIndex + 0);
        m_Indices.push_back(baseIndex + 3);
        m_Indices.push_back(baseIndex + 2);
    }
}