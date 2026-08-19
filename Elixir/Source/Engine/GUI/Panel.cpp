#include "epch.h"
#include "Panel.h"

#include <Engine/GUI/Canvas.h>

namespace Elixir::GUI
{
    void Panel::Update(const Timestep frameTime)
    {
        for (size_t i = 0; i < GetSlotCount(); ++i)
        {
            if (const Slot* slot = GetSlotAt(i); slot->IsVisible())
                slot->GetWidget()->Update(frameTime);
        }
    }

    void Panel::ClearChildren()
    {
        if (GetSlotCount() == 0) return;

        for (size_t i = 0; i < GetSlotCount(); ++i)
            DetachChild(GetSlotAt(i)->GetWidget());

        ClearSlots();
        MarkLayoutDirty();
    }

    void Panel::SetPadding(const SPadding& padding)
    {
        if (m_Padding == padding) return;
        m_Padding = padding;
        MarkLayoutDirty();
    }

    void Panel::SetBackground(const SColor& color)
    {
        m_Background = color;
        MarkRenderDirty();
    }

    void Panel::SetCornerRadius(const glm::vec4& radius)
    {
        m_CornerRadius = radius;
        MarkRenderDirty();
    }

    Ref<Widget> Panel::GetChildAt(const size_t index) const
    {
        if (index >= GetSlotCount()) return nullptr;
        return GetSlotAt(index)->GetWidget();
    }

    void Panel::RemoveChild(const Ref<Widget>& child)
    {
        if (!child) return;

        for (size_t i = 0; i < GetSlotCount(); ++i)
        {
            if (GetSlotAt(i)->GetWidget() == child)
            {
                RemoveSlotAt(i);
                DetachChild(child);
                break;
            }
        }
    }

    void Panel::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
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

    template class ELIXIR_API TPanel<LayoutSlot>;
    template class ELIXIR_API TPanel<CanvasSlot>;
}
