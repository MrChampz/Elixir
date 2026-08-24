#include "epch.h"
#include "TextBlock.h"

#include <Engine/Font/FontManager.h>

namespace Elixir::GUI
{
    namespace
    {
        std::string GetFirstLine(const std::string& text)
        {
            return text.substr(0, text.find_first_of("\r\n"));
        }
    }

    TextBlock::TextBlock(const std::string& text)
      : m_Text(text),
        m_DisplayText(GetFirstLine(text))
    {
        m_Font = FontManager::GetDefaultFont();
    }

    void TextBlock::SetText(const std::string& text)
    {
        if (m_Text == text) return;
        m_Text = text;
        m_DisplayText = m_Overflow == ETextOverflow::Wrap ? text : GetFirstLine(text);
        MarkLayoutDirty();
        MarkRenderDirty(); // the drawn glyphs change even when geometry does not
    }

    void TextBlock::SetFont(const Ref<Font>& font)
    {
        EE_CORE_ASSERT(font, "TextBlock::SetFont called with a null font");
        if (!font || m_Font == font) return;

        m_Font = font;
        m_DisplayText = m_Overflow == ETextOverflow::Wrap ? m_Text : GetFirstLine(m_Text);
        MarkLayoutDirty();
        MarkRenderDirty();
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
        m_DisplayText = m_Overflow == ETextOverflow::Wrap ? m_Text : GetFirstLine(m_Text);
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void TextBlock::SetOverflow(const ETextOverflow overflow)
    {
        if (m_Overflow == overflow) return;
        m_Overflow = overflow;
        m_DisplayText = overflow == ETextOverflow::Wrap ? m_Text : GetFirstLine(m_Text);
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    glm::vec2 TextBlock::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        if (m_Overflow == ETextOverflow::Wrap)
            return UpdateWrappedDisplayText(availableSize.x);

        m_DisplayText = GetFirstLine(m_Text);

        return FontManager::MeasureText(m_DisplayText, m_Font, m_FontSize);
    }

    void TextBlock::LayoutChildren(const SRect& allocatedSpace)
    {
        if (m_Overflow == ETextOverflow::Ellipsis)
            m_DisplayText = EllipsizeText(GetFirstLine(m_Text), allocatedSpace.Size.x, false);
        else if (m_Overflow == ETextOverflow::Wrap)
            UpdateWrappedDisplayText(allocatedSpace.Size.x, allocatedSpace.Size.y);
        else
            m_DisplayText = ClipText(GetFirstLine(m_Text), allocatedSpace.Size.x);
    }

    void TextBlock::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        if (m_DisplayText.empty()) return;
        batch.AddText(
            m_DisplayText,
            m_Geometry,
            m_Font,
            m_FontSize,
            m_Color,
            zOrder
        );
    }

    std::string TextBlock::ClipText(
        const std::string& text,
        const float availableWidth
    ) const
    {
        if (availableWidth <= 0.0f) return {};

        std::string clipped = text;
        while (!clipped.empty()
            && FontManager::MeasureText(clipped, m_Font, m_FontSize).x > availableWidth)
        {
            UTF8::UTF8RemoveLastChar(clipped);
        }
        return clipped;
    }

    std::string TextBlock::EllipsizeText(
        const std::string& text,
        const float availableWidth,
        const bool appendEllipsis
    ) const
    {
        if (!appendEllipsis && FontManager::MeasureText(text, m_Font, m_FontSize).x <= availableWidth)
            return text;

        constexpr std::string_view ellipsis = "...";
        const float ellipsisWidth = FontManager::MeasureText(std::string(ellipsis), m_Font, m_FontSize).x;
        if (ellipsisWidth > availableWidth)
            return {};

        std::string truncated = text;
        while (!truncated.empty())
        {
            UTF8::UTF8RemoveLastChar(truncated);
            const float truncatedWidth = FontManager::MeasureText(truncated, m_Font, m_FontSize).x;
            if (truncatedWidth + ellipsisWidth <= availableWidth)
                return truncated + std::string(ellipsis);
        }

        return std::string(ellipsis);
    }

    glm::vec2 TextBlock::UpdateWrappedDisplayText(const float maxWidth, const float maxHeight)
    {
        std::vector<std::string> lines;
        FontManager::MeasureWrapped(
            m_Text,
            m_Font,
            m_FontSize,
            maxWidth,
            &lines
        );

        size_t visibleLineCount = lines.size();
        const float lineHeight = FontManager::GetLineHeight(m_Font, m_FontSize);
        if (maxHeight != UnconstrainedSize && lineHeight > 0.0f)
        {
            visibleLineCount = std::min(
                visibleLineCount,
                static_cast<size_t>(std::floor(std::max(0.0f, maxHeight) / lineHeight))
            );
        }

        if (visibleLineCount < lines.size() && visibleLineCount > 0)
            lines[visibleLineCount - 1] = EllipsizeText(lines[visibleLineCount - 1], maxWidth, true);

        m_DisplayText.clear();
        float widestLine = 0.0f;
        for (size_t i = 0; i < visibleLineCount; ++i)
        {
            if (i > 0) m_DisplayText += '\n';
            m_DisplayText += lines[i];
            widestLine = std::max(
                widestLine,
                FontManager::MeasureText(lines[i], m_Font, m_FontSize).x
            );
        }

        return { widestLine, lineHeight * visibleLineCount };
    }
}
