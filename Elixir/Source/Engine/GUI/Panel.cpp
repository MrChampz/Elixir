#include "epch.h"
#include "Panel.h"

namespace Elixir::GUI
{
    void Panel::Update(const Timestep frameTime)
    {
        for (const auto& slot : m_Slots)
        {
            if (slot->IsVisible())
            {
                slot->GetWidget()->Update(frameTime);
            }
        }
    }

    void Panel::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        for (const auto& slot : m_Slots)
        {
            if (slot->IsVisible())
            {
                slot->GetWidget()->GenerateDrawCommands(batch, zOrder + 1);
            }
        }

        if (m_Background.A > 0.0f)
        {
            batch.AddRect(
                m_Geometry,
                m_Background,
                m_CornerRadius,
                m_InsetShadow,
                m_DropShadow,
                m_Outline,
                zOrder
            );
        }
    }
}