#include "epch.h"
#include "Button.h"

namespace Elixir::GUI
{
    Button::Button()
    {
        m_DesiredSize = { 120.0f, 40.0f };
    }

    void Button::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        auto buttonColor = m_NormalColor;

        // Background
        batch.AddRect(m_Geometry, buttonColor, zOrder);
    }
}