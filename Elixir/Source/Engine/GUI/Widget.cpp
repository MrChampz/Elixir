#include "epch.h"
#include "Widget.h"

#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    /* Widget */

    void Widget::ArrangeChildren(const SRect& allocatedSpace)
    {
        if (!m_LayoutDirty && m_LastArrangedSpace == allocatedSpace)
            return;

        m_Geometry = allocatedSpace;
        m_LastArrangedSpace = allocatedSpace;

        LayoutChildren(allocatedSpace);

        m_LayoutDirty = false;
    }

    void Widget::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        m_RenderDirty = false;
        Draw(batch, zOrder);
    }

    void Widget::SetOpacity(const float opacity)
    {
        m_Opacity = opacity;
        MarkRenderDirty();
    }

    void Widget::SetVisibility(const EVisibility visibility)
    {
        if (m_Visibility == visibility) return;
        m_Visibility = visibility;
        MarkLayoutDirty();
    }

    bool Widget::IsVisible() const
    {
        return m_Visibility == EVisibility::Visible && m_Opacity > 0.0f;
    }

    void Widget::SetInsetShadow(const glm::vec4& shadow)
    {
        m_InsetShadow = shadow;
        MarkRenderDirty();
    }

    void Widget::SetInsetShadowOffset(const glm::vec2& offset)
    {
        m_InsetShadow.x = offset.x;
        m_InsetShadow.y = offset.y;
        MarkRenderDirty();
    }

    void Widget::SetInsetShadowBlur(const float blur)
    {
        m_InsetShadow.z = blur;
        MarkRenderDirty();
    }

    void Widget::SetInsetShadowIntensity(const float intensity)
    {
        m_InsetShadow.w = intensity;
        MarkRenderDirty();
    }

    void Widget::SetDropShadow(const glm::vec4& shadow)
    {
        m_DropShadow = shadow;
        MarkRenderDirty();
    }

    void Widget::SetDropShadowOffset(const glm::vec2& offset)
    {
        m_DropShadow.x = offset.x;
        m_DropShadow.y = offset.y;
        MarkRenderDirty();
    }

    void Widget::SetDropShadowBlur(const float blur)
    {
        m_DropShadow.z = blur;
        MarkRenderDirty();
    }

    void Widget::SetDropShadowIntensity(const float intensity)
    {
        m_DropShadow.w = intensity;
        MarkRenderDirty();
    }

    void Widget::SetOutline(const SOutline& outline)
    {
        m_Outline = outline;
        MarkRenderDirty();
    }

    void Widget::SetOutlineColor(const SColor& color)
    {
        m_Outline.Color = color;
        MarkRenderDirty();
    }

    void Widget::SetOutlineThickness(const float thickness)
    {
        m_Outline.Thickness = thickness;
        MarkRenderDirty();
    }

    void Widget::AttachChild(const Ref<Widget>& child)
    {
        EE_CORE_ASSERT(!weak_from_this().expired(), "Parent must be owned by a Ref before adopting")

        if (child)
        {
            if (const auto prev = child->m_Parent.lock(); prev && prev != shared_from_this())
                prev->RemoveChild(child);

            child->m_Parent = weak_from_this();
            MarkLayoutDirty();
        }
    }

    void Widget::DetachChild(const Ref<Widget>& child)
    {
        if (child && child->m_Parent.lock().get() == this)
        {
            child->m_Parent.reset();
            MarkLayoutDirty();
        }
    }

    void Widget::MarkLayoutDirty()
    {
        if (m_LayoutDirty)
            return;

        m_LayoutDirty = true;

        if (const auto parent = m_Parent.lock())
            parent->MarkLayoutDirty();
    }

    void Widget::MarkRenderDirty()
    {
        m_RenderDirty = true;
    }

    void Widget::HandleMouseEnter()
    {
        m_Hovered = true;
        if (m_OnMouseEnterCallback) m_OnMouseEnterCallback();
    }

    void Widget::HandleMouseLeave()
    {
        m_Hovered = false;
        if (m_OnMouseLeaveCallback) m_OnMouseLeaveCallback();
    }

    void Widget::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        m_Pressed = true;
        if (m_OnMouseDownCallback) m_OnMouseDownCallback();
    }

    void Widget::HandleMouseUp(const MouseButtonReleasedEvent& event)
    {
        if (m_Pressed)
        {
            if (m_OnMouseUpCallback)
                m_OnMouseUpCallback();

            if (m_Hovered)
                HandleClick();
        }

        m_Pressed = false;
    }

    void Widget::HandleFocus()
    {
        m_Focused = true;
        if (m_OnFocusCallback) m_OnFocusCallback();
    }

    void Widget::HandleLostFocus()
    {
        m_Focused = false;
        if (m_OnLostFocusCallback) m_OnLostFocusCallback();
    }

    void Widget::HandleClick()
    {
        if (m_OnClickCallback) m_OnClickCallback();
    }

    SRect Widget::ApplyPadding(const SRect& availableSpace, const SPadding& padding)
    {
        SRect result;
        result.Position.x = availableSpace.Position.x + padding.Left;
        result.Position.y = availableSpace.Position.y + padding.Top;
        result.Size.x = availableSpace.Size.x - padding.GetTotalHorizontal();
        result.Size.y = availableSpace.Size.y - padding.GetTotalVertical();

        return result;
    }

    SRect Widget::ApplyMargin(const SRect& availableSpace, const SMargin& margin)
    {
        SRect result;
        result.Position.x = availableSpace.Position.x + margin.Left;
        result.Position.y = availableSpace.Position.y + margin.Top;
        result.Size.x = availableSpace.Size.x - margin.GetTotalHorizontal();
        result.Size.y = availableSpace.Size.y - margin.GetTotalVertical();

        return result;
    }

    SRect Widget::AlignChild(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EHorizontalAlignment hAlignment,
        const EVerticalAlignment vAlignment,
        const SMargin& margin
    )
    {
        SRect result = ApplyMargin(availableSpace, margin);
        result = AlignHorizontally(childSize, result, hAlignment);
        result = AlignVertically(childSize, result, vAlignment);

        return result;
    }

    SRect Widget::AlignHorizontally(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EHorizontalAlignment alignment
    )
    {
        SRect result = availableSpace;

        switch (alignment)
        {
            case EHorizontalAlignment::Left:
                result.Position.x = availableSpace.Position.x;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Center:
                result.Position.x = availableSpace.Position.x + (availableSpace.Size.x - childSize.x) * 0.5f;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Right:
                result.Position.x = availableSpace.Position.x + availableSpace.Size.x - childSize.x;
                result.Size.x = childSize.x;
                break;
        }

        return result;
    }

    SRect Widget::AlignVertically(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EVerticalAlignment alignment
    )
    {
        SRect result = availableSpace;

        switch (alignment)
        {
            case EVerticalAlignment::Top:
                result.Position.y = availableSpace.Position.y;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Center:
                result.Position.y = availableSpace.Position.y + (availableSpace.Size.y - childSize.y) * 0.5f;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Bottom:
                result.Position.y = availableSpace.Position.y + availableSpace.Size.y - childSize.y;
                result.Size.y = childSize.y;
                break;
        }

        return result;
    }

    /* ContentWidget */

    void ContentWidget::Update(const Timestep frameTime)
    {
        if (m_ContentSlot && m_ContentSlot->IsVisible())
        {
            m_ContentSlot->GetWidget()->Update(frameTime);
        }
    }

    ContentSlot& ContentWidget::SetContent(const Ref<Widget>& widget)
    {
        ClearContent();

        // A null widget means "no content". Guard against it so we never leave a
        // phantom slot that makes HasContent() report true with no widget to
        // draw/update. Callers that want to empty the content should use
        // ClearContent(); reaching here with null is a misuse.
        EE_CORE_ASSERT(widget, "SetContent called with a null widget; use ClearContent to empty content");
        if (!widget)
        {
            // Release fallback (asserts are compiled out): keep the slot empty and
            // return an inert sentinel so HasContent() stays truthful.
            static ContentSlot emptyContent{nullptr};
            return emptyContent;
        }

        m_ContentSlot = CreateRef<ContentSlot>(widget);
        AttachChild(widget);
        return *m_ContentSlot;
    }

    void ContentWidget::ClearContent()
    {
        if (!m_ContentSlot) return;

        DetachChild(m_ContentSlot->GetWidget());
        m_ContentSlot.reset();
    }

    void ContentWidget::RemoveChild(const Ref<Widget>& child)
    {
        if (m_ContentSlot && m_ContentSlot->GetWidget() == child)
            ClearContent();
    }

    void ContentWidget::ForEachChild(const std::function<void(const Ref<Widget>&)>& fn) const
    {
        if (m_ContentSlot)
        {
            if (const auto& child = m_ContentSlot->GetWidget())
                fn(child);
        }
    }
}
