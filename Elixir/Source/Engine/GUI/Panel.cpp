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

    void Panel::VisitChildren(const std::function<void(const Ref<Widget>&)>& visitor) const
    {
        for (const auto& slot : m_Slots)
        {
            if (const auto& child = slot->GetWidget())
                visitor(child);
        }
    }

    void Panel::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        // Children are walked separately by CollectDrawCommands; a panel only draws its background.
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