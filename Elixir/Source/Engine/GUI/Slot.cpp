#include "epch.h"
#include "Slot.h"

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    /* Slot */

    bool Slot::IsVisible() const
    {
        return m_Widget && m_Widget->IsVisible();
    }

    void Slot::InvalidateOwnerLayout() const
    {
        if (m_Widget)
            if (const auto parent = m_Widget->GetParent())
                parent->MarkLayoutDirty();
    }

    /* ContentSlot */

    ContentSlot& ContentSlot::SetHorizontalAlignment(const EHorizontalAlignment alignment)
    {
        m_HAlignment = alignment;
        InvalidateOwnerLayout();
        return *this;
    }

    ContentSlot& ContentSlot::SetVerticalAlignment(const EVerticalAlignment alignment)
    {
        m_VAlignment = alignment;
        InvalidateOwnerLayout();
        return *this;
    }

    ContentSlot& ContentSlot::SetMargin(const SMargin& margin)
    {
        m_Margin = margin;
        InvalidateOwnerLayout();
        return *this;
    }

    /* LayoutSlot */

    LayoutSlot& LayoutSlot::SetHorizontalAlignment(const EHorizontalAlignment alignment)
    {
        m_HAlignment = alignment;
        InvalidateOwnerLayout();
        return *this;
    }

    LayoutSlot& LayoutSlot::SetVerticalAlignment(const EVerticalAlignment alignment)
    {
        m_VAlignment = alignment;
        InvalidateOwnerLayout();
        return *this;
    }

    LayoutSlot& LayoutSlot::SetMargin(const SMargin& margin)
    {

        m_Margin = margin;
        InvalidateOwnerLayout();
        return *this;
    }

    LayoutSlot& LayoutSlot::SetMinSize(const glm::vec2& size)
    {
        m_MinSize = size;
        InvalidateOwnerLayout();
        return *this;
    }

    LayoutSlot& LayoutSlot::SetMaxSize(const glm::vec2& size)
    {
        m_MaxSize = size;
        InvalidateOwnerLayout();
        return *this;
    }

    LayoutSlot& LayoutSlot::SetFillRatio(const float ratio)
    {
        m_FillRatio = ratio;
        InvalidateOwnerLayout();
        return *this;
    }
}