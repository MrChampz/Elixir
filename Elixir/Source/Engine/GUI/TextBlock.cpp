#include "epch.h"
#include "TextBlock.h"

#include <Engine/Font/FontManager.h>

namespace Elixir::GUI
{
    TextBlock::TextBlock(const std::string& text)
      : m_Text(text),
        m_DisplayText(text)
    {
        m_Font = FontManager::GetDefaultFont();
    }

    void TextBlock::SetText(const std::string& text)
    {
        if (m_Text == text) return;
        m_Text = text;
        m_DisplayText = text;
        MarkLayoutDirty();
        MarkRenderDirty(); // the drawn glyphs change even when geometry does not
    }

    void TextBlock::SetFont(const Ref<Font>& font)
    {
        EE_CORE_ASSERT(font, "TextBlock::SetFont called with a null font");
        if (!font || m_Font == font) return;

        m_Font = font;
        m_DisplayText = m_Text;
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
        m_DisplayText = m_Text;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void TextBlock::SetOverflow(const ETextOverflow overflow)
    {
        if (m_Overflow == overflow) return;
        m_Overflow = overflow;
        m_DisplayText = m_Text;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    glm::vec2 TextBlock::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        if (m_Overflow == ETextOverflow::Wrap && availableSize.x != UnconstrainedSize)
            return UpdateWrappedDisplayText(availableSize.x);

        m_DisplayText = m_Text;

        return FontManager::MeasureText(m_Text, m_Font, m_FontSize);
    }

    void TextBlock::LayoutChildren(const SRect& allocatedSpace)
    {
        if (m_Overflow == ETextOverflow::Ellipsis)
            m_DisplayText = ProcessText(m_Text, allocatedSpace.Size.x);
        else if (m_Overflow == ETextOverflow::Wrap)
            UpdateWrappedDisplayText(allocatedSpace.Size.x);

        // Clip: m_DisplayText already holds the untruncated text; clipping to m_Geometry is
        // a draw-time concern, not a string concern.
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

    std::string TextBlock::ProcessText(
        const std::string& text,
        const float availableWidth
    ) const
    {
        auto size = FontManager::MeasureText(text, m_Font, m_FontSize);
        if (size.x <= availableWidth)
            return text;

        std::string ellipsis = "...";
        const float ellipsisWidth = FontManager::MeasureText(ellipsis, m_Font, m_FontSize).x;

        if (ellipsisWidth >= availableWidth)
            return ellipsis;

        std::string truncated = text;
        while (!truncated.empty())
        {
            UTF8::UTF8RemoveLastChar(truncated);
            size = FontManager::MeasureText(truncated, m_Font, m_FontSize);
            if (size.x + ellipsisWidth <= availableWidth)
                return truncated + ellipsis;
        }

        return ellipsis;
    }

    glm::vec2 TextBlock::UpdateWrappedDisplayText(float maxWidth)
    {
        std::vector<std::string> lines;
        const glm::vec2 size = FontManager::MeasureWrapped(
            m_Text,
            m_Font,
            m_FontSize,
            maxWidth,
            &lines
        );

        m_DisplayText.clear();
        for (size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0) m_DisplayText += '\n';
            m_DisplayText += lines[i];
        }

        return size;
    }
}
