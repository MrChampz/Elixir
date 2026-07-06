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

    void Panel::RemoveChild(const Ref<Widget>& child)
    {
        if (!child) return;

        const auto it = std::ranges::find_if(
            m_Slots,
            [&](const Ref<Slot>& slot) { return slot->GetWidget() == child; }
        );

        if (it == m_Slots.end()) return;

        m_Slots.erase(it);
        DetachChild(child);
    }

    void Panel::ClearChildren()
    {
        if (m_Slots.empty()) return;

        for (const auto& slot : m_Slots)
            DetachChild(slot->GetWidget());

        m_Slots.clear();
        MarkLayoutDirty();
    }

    void Panel::SetPadding(const SPadding& padding)
    {
        m_Padding = padding;
        MarkLayoutDirty();
    }
}
