#include "epch.h"
#include "Button.h"

#include <Engine/Font/FontManager.h>
#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    Button::Button(const std::string& text)
        : m_Text(text)
    {
        m_Font = FontManager::GetDefaultFont();
        m_DesiredSize = { 120.0f, 40.0f };
    }

    glm::vec2 Button::ComputeDesiredSize()
    {
        return m_DesiredSize;
    }

    void Button::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        auto buttonColor = m_NormalColor;

        if (m_Hovered)
        {
            buttonColor = m_HoverColor;
        }

        // Background
        if (m_NormalBackground)
        {
            batch.AddTexture(m_NormalBackground, m_Geometry, buttonColor, zOrder);
        }
        else
        {
            batch.AddRect(
                m_Geometry,
                buttonColor,
                m_CornerRadius,
                m_InsetShadow,
                m_DropShadow,
                m_Outline,
                zOrder
            );
        }

        if (HasContent())
        {
            m_ContentSlot->GetWidget()->GenerateDrawCommands(batch, zOrder + 1);
        }
        else if (!m_Text.empty())
        {
            const float availableWidth = m_Geometry.Size.x - m_Padding.GetTotalHorizontal();

            const auto displayText = ProcessText(m_Text, availableWidth);
            const auto textSize = MeasureTextSize(displayText);
            const auto textPos = CalculateTextPosition(textSize);

            batch.AddText(
                displayText,
                { textPos, textSize },
                m_Font,
                m_FontSize,
                m_TextColor,
                zOrder + 1,
                m_Geometry
            );
        }
    }

    void Button::ArrangeChildren(const SRect& allocatedSpace)
    {
        m_Geometry = allocatedSpace;

        if (HasContent())
        {
            const glm::vec2 childSize = m_ContentSlot->GetWidget()->ComputeDesiredSize();
            const SRect innerSpace = ApplyPadding(allocatedSpace, m_Padding);

            const SRect childRect  = AlignChild(
                childSize,
                innerSpace,
                m_ContentSlot->GetHorizontalAlignment(),
                m_ContentSlot->GetVerticalAlignment(),
                m_ContentSlot->GetMargin()
            );

            m_ContentSlot->GetWidget()->ArrangeChildren(childRect);
        }
    }

    std::string Button::ProcessText(const std::string& text, const float availableWidth)
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

    glm::vec2 Button::MeasureTextSize(const std::string& text)
    {
        return FontManager::MeasureText(text, m_Font, m_FontSize);
    }

    glm::vec2 Button::CalculateTextPosition(const glm::vec2 textSize)
    {
        const glm::vec2 contentPos = m_Geometry.Position + glm::vec2(
            m_Padding.Left,
            m_Padding.Top
        );
        const glm::vec2 contentSize = m_Geometry.Size - glm::vec2(
            m_Padding.GetTotalHorizontal(),
            m_Padding.GetTotalVertical()
        );

        return contentPos + (contentSize - textSize) * 0.5f;
    }

    void Button::HandleMouseEnter()
    {
        Widget::HandleMouseEnter();
        // TODO: Show pointer cursor
    }

    void Button::HandleMouseLeave()
    {
        Widget::HandleMouseLeave();
        // TODO: Show default cursor
    }
}