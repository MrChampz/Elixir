#include "epch.h"
#include "Button.h"

#include "imgui_internal.h"

#include <Engine/Font/FontManager.h>

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

        if (m_IsHovered)
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

        batch.AddDebugRect(m_Geometry);

        // Text (centered)
        if (!m_Text.empty())
        {
            const float availableWidth = m_Geometry.Size.x - m_Padding.GetTotalHorizontal();

            const auto displayText = ProcessText(m_Text, availableWidth);
            const auto textSize = MeasureTextSize(displayText);
            const auto textPos = CalculateTextPosition(textSize);

            batch.AddDebugRect({ textPos, textSize });
            batch.AddText(displayText, { textPos, textSize }, m_FontSize, m_TextColor, zOrder + 1);
        }
    }

    std::string Button::ProcessText(const std::string& text, const float availableWidth)
    {
        const float charWidth = m_FontSize * 0.38f; // TODO: Get chat width from font metrics
        const int maxChars = int(availableWidth / charWidth);

        if (text.length() > maxChars)
        {
            return text.substr(0, maxChars - 3) + "...";
        }

        return text;
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