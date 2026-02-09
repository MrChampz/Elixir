#include "epch.h"
#include "Button.h"

namespace Elixir::GUI
{
    Button::Button(const std::string& text)
        : m_Text(text)
    {
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

        // Text (centered)
        if (!m_Text.empty())
        {
            auto textPos = m_Geometry.Position + m_Geometry.Size;
            textPos.x -= (m_Text.length() * 18.0f) * 0.5f;
            textPos.y -= m_Geometry.Size.y * 0.4f;

            batch.AddText(m_Text, textPos, 16.0f, m_TextColor, zOrder + 1);
        }
    }
}