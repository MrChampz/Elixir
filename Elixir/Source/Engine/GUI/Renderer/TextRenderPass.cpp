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
        m_Quads.clear();

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

        if (!m_Quads.empty())
        {
            m_QuadBuffer->UpdateData(m_Quads.data(),  m_Quads.size() * sizeof(SQuad));
        }
    }

    void TextRenderPass::Render(const Ref<CommandBuffer>& cmd)
    {
        m_Pipeline->Bind(cmd);
        m_QuadBuffer->Bind(cmd);
        cmd->Draw(6, m_Quads.size());
    }

    bool TextRenderPass::HasData() const
    {
        return !m_Quads.empty();
    }

    void TextRenderPass::Clear()
    {
        m_Quads.clear();
    }

    void TextRenderPass::InitFontData()
    {
        m_Font = FontManager::Load("./Assets/Fonts/SF-Pro-Display-Regular.otf");

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
                    { EDataType::Vec2, "Position"  },
                    { EDataType::Vec2, "Size"      },
                    { EDataType::Vec4, "TexCoords" },
                    { EDataType::Vec4, "Color"     },
                },
                EInputRate::Instance
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

        m_Quads.reserve(MAX_CHARACTERS);
        m_QuadBuffer = DynamicVertexBuffer::Create(m_GraphicsContext, MAX_CHARACTERS * sizeof(SQuad));
        m_QuadBuffer->SetLayout(bufferLayout);

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
        m_Shader->BindTexture("mtsdf", m_Font->Atlas.MTSDF);

        const auto sampler = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Linear)
            .SetMinFilter(ESamplerFilter::Linear)
            .Build(m_GraphicsContext);
        m_Shader->BindSampler("atlasSampler", sampler);
    }

    void TextRenderPass::BuildTextGeometry(const SDrawCommand& cmd)
    {
        const float ascenderY = m_Font->Atlas.Info.AscenderY;
        const float descenderY = m_Font->Atlas.Info.DescenderY;
        const float scale = 1.0f / (ascenderY - descenderY);
        const float lineHeight = scale * (ascenderY - descenderY) * cmd.FontSize;

        float cursorX = cmd.Geometry.Position.x;
        const float cursorY = cmd.Geometry.Position.y + (cmd.Geometry.Size.y - lineHeight) * 0.5f;

        for (const char c : cmd.Text)
        {
            auto glyph = m_Font->GetGlyph(c);

            if (glyph.has_value() && glyph.value().PlaneBounds.has_value())
            {
                SDrawCommand charCmd = cmd;

                const auto& planeBounds = glyph.value().PlaneBounds.value();

                const float glyphWidth = planeBounds.Size.x - planeBounds.Position.x;
                const float glyphHeight = planeBounds.Size.y - planeBounds.Position.y;

                const float bearingX = planeBounds.Position.x;
                const float bearingY = planeBounds.Size.y;

                charCmd.Geometry.Position.x = cursorX + scale * bearingX * cmd.FontSize;
                charCmd.Geometry.Position.y = cursorY + scale * (ascenderY - bearingY) * cmd.FontSize;
                charCmd.Geometry.Size.x     = glyphWidth * scale * cmd.FontSize;
                charCmd.Geometry.Size.y     = glyphHeight * scale * cmd.FontSize;

                const auto& atlasBounds = glyph.value().AtlasBounds.value();

                glm::vec2 texCoordMin = atlasBounds.Position;
                glm::vec2 texCoordMax = atlasBounds.Size;

                const float texelWidth = 1.0f / m_Font->Atlas.Info.Width;
                const float texelHeight = 1.0f / m_Font->Atlas.Info.Height;
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
        const SQuad quad = {
            .Position = cmd.Geometry.Position,
            .Size = cmd.Geometry.Size,
            .TexCoords = cmd.TexCoords,
            .Color = cmd.Color,
        };

        m_Quads.push_back(quad);
    }
}