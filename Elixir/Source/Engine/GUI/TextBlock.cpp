#include "epch.h"
#include "TextBlock.h"

#include <Engine/Font/FontManager.h>

namespace Elixir::GUI
{
    TextBlock::TextBlock(const std::string& text)
        : m_Text(text)
    {
        m_Font = FontManager::GetDefaultFont();
        UpdateTextSize();
    }

    glm::vec2 TextBlock::ComputeDesiredSize()
    {
        return m_DesiredSize;
    }

    void TextBlock::SetText(const std::string& text)
    {
        if (m_Text == text) return;
        m_Text = text;
        UpdateTextSize();
        MarkLayoutDirty();
    }

    void TextBlock::SetFont(const Ref<Font>& font)
    {
        if (m_Font == font) return;
        m_Font = font;
        UpdateTextSize();
        MarkLayoutDirty();
    }

    void TextBlock::SetColor(const SColor& color)
    {
        m_Color = color;
        MarkRenderDirty();
    }

    void TextBlock::SetFontSize(const float size)
    {
        if (m_FontSize == size) return;
        m_FontSize = size;
        UpdateTextSize();
        MarkLayoutDirty();
    }

    void TextBlock::Draw(RenderBatch& batch, const int zOrder)
    {
        if (!m_Text.empty())
        {
            const float availableWidth = m_Geometry.Size.x;
            const auto displayText = ProcessText(m_Text, availableWidth);

            batch.AddText(
                displayText,
                m_Geometry,
                m_Font,
                m_FontSize,
                m_Color,
                zOrder
            );
        }
    }

    void TextBlock::UpdateTextSize()
    {
        m_DesiredSize = FontManager::MeasureText(m_Text, m_Font, m_FontSize);
    }

    std::string TextBlock::ProcessText(
        const std::string& text,
        const float availableWidth
    ) const
    {
        auto size = FontManager::MeasureText(text, m_Font, m_FontSize);
        if (size.x <= availableWidth)
            return text;

        const std::string ellipsis = "...";
        const float ellipsisWidth = FontManager::MeasureText(ellipsis, m_Font, m_FontSize).x;

        if (ellipsisWidth >= availableWidth)
            return ellipsis;

        std::string truncated = text;
        while (!truncated.empty())
        {
            truncated.pop_back();
            size = FontManager::MeasureText(truncated, m_Font, m_FontSize);
            if (size.x + ellipsisWidth <= availableWidth)
                return truncated + ellipsis;
        }

        return ellipsis;
    }
}