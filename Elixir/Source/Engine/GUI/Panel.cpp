#include "epch.h"
#include "Panel.h"

#include <Engine/GUI/Canvas.h>

namespace Elixir::GUI
{
    namespace
    {
        bool HasShadow(const glm::vec4& shadow)
        {
            return shadow.w > 0.0f && (shadow.x != 0.0f || shadow.y != 0.0f);
        }

        bool HasVisualOutput(const SBrush& brush)
        {
            return brush.Color.A > 0.0f ||
                brush.Texture ||
                (brush.Outline.Thickness > 0.0f && brush.Outline.Color.A > 0.0f) ||
                HasShadow(brush.InsetShadow) ||
                HasShadow(brush.DropShadow);
        }
    }

    void Panel::Update(const Timestep frameTime)
    {
        Widget::Update(frameTime);
        for (size_t i = 0; i < GetSlotCount(); ++i)
        {
            if (const Slot* slot = GetSlotAt(i))
            {
                const auto child = slot->GetWidget();
                if (child && child->TakesSpace())
                    child->Update(frameTime);
            }
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
                DetachChild(child);
                RemoveSlotAt(i);
                break;
            }
        }
    }

    void Panel::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        const SBrush& brush = GetResolvedAppearance().Background;

        if (HasVisualOutput(brush))
            batch.AddBrush(brush, m_Geometry, zOrder);
    }

    template class ELIXIR_API TPanel<LayoutSlot>;
    template class ELIXIR_API TPanel<CanvasSlot>;
}
