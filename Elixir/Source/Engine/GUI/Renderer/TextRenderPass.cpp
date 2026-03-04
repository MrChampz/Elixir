#include "epch.h"
#include "TextRenderPass.h"

#include <Engine/Font/FontManager.h>
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

    void TextRenderPass::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec2, "Position"    },
                    { EDataType::Vec2, "Size"        },
                    { EDataType::Vec4, "TexCoords"   },
                    { EDataType::Vec4, "Color"       },
                    { EDataType::UInt, "AtlasIndex"  },
                    { EDataType::Vec2, "UnitRange"   },
                    { EDataType::Vec4, "ScissorRect" },
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
    }

    void TextRenderPass::BindShaderParameters() const
    {
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
        m_Shader->BindTextureSet("atlases", FontManager::GetAtlasesTextureSet());

        const auto sampler = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Linear)
            .SetMinFilter(ESamplerFilter::Linear)
            .Build(m_GraphicsContext);
        m_Shader->BindSampler("atlasSampler", sampler);
    }

    void TextRenderPass::BuildTextGeometry(const SDrawCommand& cmd)
    {
        const auto font = cmd.Font;

        const float ascenderY = font->GetAscenderY();
        const float scale = font->GetScale();
        const float lineHeight = FontManager::GetLineHeight(font, cmd.FontSize);

        float cursorX = cmd.Geometry.Position.x;
        const float cursorY = cmd.Geometry.Position.y + (cmd.Geometry.Size.y - lineHeight) * 0.5f;

        int i = 0;
        while (i < (int)cmd.Text.size())
        {
            const auto charLen = UTF8::UTF8CharLength(cmd.Text[i]);
            const auto codepoint = UTF8::UTF8ToCodepoint(cmd.Text, i);

            auto glyph = font->GetGlyph(codepoint);

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

                const auto atlas = font->GetAtlas();
                const float texelWidth = 1.0f / atlas.Info.Width;
                const float texelHeight = 1.0f / atlas.Info.Height;
                texCoordMin *= glm::vec2(texelWidth, texelHeight);
                texCoordMax *= glm::vec2(texelWidth, texelHeight);

                charCmd.TexCoords.Position = texCoordMin;
                charCmd.TexCoords.Size     = texCoordMax;

                charCmd.TexCoords.Position.y = 1.0f - charCmd.TexCoords.Position.y;
                charCmd.TexCoords.Size.y = 1.0f - charCmd.TexCoords.Size.y;

                BuildTextureGeometry(charCmd);
                cursorX += scale * glyph->Advance * cmd.FontSize;
            }
            i += charLen;
        }
    }

    void TextRenderPass::BuildTextureGeometry(const SDrawCommand& cmd)
    {
        const SQuad quad = {
            .Position = cmd.Geometry.Position,
            .Size = cmd.Geometry.Size,
            .TexCoords = cmd.TexCoords,
            .Color = cmd.Color,
            .AtlasIndex = cmd.Font->GetAtlasHandle().Index,
            .UnitRange = cmd.Font->GetUnitRange(),
            .ScissorRect = cmd.ScissorRect
        };

        m_Quads.push_back(quad);
    }
}