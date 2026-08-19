#include "epch.h"
#include "Widget.h"

#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    /* Widget */

    const glm::vec2& Widget::Measure(const glm::vec2& availableSize)
    {
        if (!m_MeasureDirty && m_LastMeasureConstraint == availableSize)
            return m_DesiredSize;

        m_DesiredSize = ComputeDesiredSize(availableSize);
        m_LastMeasureConstraint = availableSize;
        m_MeasureDirty = false;

        return m_DesiredSize;
    }

    void Widget::ArrangeChildren(const SRect& allocatedSpace)
    {
        if (!m_LayoutDirty && m_LastArrangedSpace == allocatedSpace)
            return;

        if (m_Geometry != allocatedSpace)
            MarkRenderDirty();

        m_Geometry = allocatedSpace;
        m_LastArrangedSpace = allocatedSpace;

        LayoutChildren(allocatedSpace);
        m_LayoutDirty = false;
    }

    void Widget::HitTest(const glm::vec2& point, std::vector<Ref<Widget>>& path)
    {
        // HitTestInvisible prunes this whole branch (neither this widget nor its children can
        // be hit); Hidden/Collapsed are not rendered/laid out, so neither should be clickable.
        if (m_Visibility == EVisibility::HitTestInvisible ||
            m_Visibility == EVisibility::Hidden ||
            m_Visibility == EVisibility::Collapsed)
            return;

        // Children sit above their parent z (see CollectDrawCommands' pre-order zCursor):
        // test the topmost child first and recurse depth-first, so the first branch that
        // reports a hit wins.
        for (size_t i = GetChildCount(); i-- > 0;)
        {
            if (const Ref<Widget> child = GetChildAt(i))
            {
                const size_t sizeBefore = path.size();
                child->HitTest(point, path);

                if (path.size() > sizeBefore)
                {
                    // SelfHitTestInvisible: this widget does not join the path, but the
                    // matched child (already appended by the recursive call) still does.
                    if (IsSelfHitTestVisible())
                        path.insert(path.begin() + sizeBefore, shared_from_this());

                    return;
                }
            }
        }

        // No child matched; this widget itself is the candidate.
        if (IsSelfHitTestVisible() && HitTestSelf(point))
            path.push_back(shared_from_this());
    }

    void Widget::SetOpacity(const float opacity)
    {
        if (m_Opacity == opacity) return;
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

    bool Widget::IsRenderVisible() const
    {
        return (m_Visibility == EVisibility::Visible ||
                m_Visibility == EVisibility::HitTestInvisible ||
                m_Visibility == EVisibility::SelfHitTestInvisible) &&
                m_Opacity > 0.0f;
    }

    bool Widget::TakesSpace() const
    {
        return m_Visibility != EVisibility::Collapsed;
    }

    bool Widget::IsSelfHitTestVisible() const
    {
        return m_Visibility == EVisibility::Visible;
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

    void Widget::ForEachChild(const std::function<void(const Ref<Widget>&)>& fn) const
    {
        for (size_t i = 0; i < GetChildCount(); ++i)
        {
            if (const Ref<Widget> child = GetChildAt(i))
                fn(child);
        }
    }

    void Widget::CollectDrawCommands(
        RenderBatch& batch,
        int& zCursor,
        bool& rebuilt,
        const SRect& clipRect
    )
    {
        if (!IsRenderVisible()) return;

        // Regenerate this widget's own commands only when its visuals/geometry changed.
        if (m_RenderDirty)
        {
            m_CachedCommands.Clear();
            BuildDrawCommands(m_CachedCommands, 0);
            m_RenderDirty = false;
            rebuilt = true;
        }

        // Own commands occupy [zCursor, zCursor + span); advance so children stack above,
        // and the next sibling starts above this whole subtree. The ancestor clip is applied
        // here, at Append time, rather than baked into m_CachedCommands: this widget's own
        // visual content and the clip it happens to sit under are independent, and
        // MarkRenderDirty never propagates to descendants (only MarkLayoutDirty does, and
        // only upward) - so nothing would tell an otherwise-unchanged widget "an ancestor's
        // clip moved, rebuild yourself". CollectDrawCommands already walks every visible
        // widget on every rebuild regardless, so intersecting the clip here costs nothing
        // extra; baking it into BuildDrawCommands would require a new downward invalidation
        // pass to avoid going stale.
        batch.Append(m_CachedCommands, zCursor, clipRect);
        zCursor += m_CachedCommands.LayerSpan();

        // A clipping container (e.g. ScrollBox) intersects its own bounds with whatever clip
        // it inherited and hands that down; everyone else just forwards the inherited clip
        // unchanged. With no inherited clip yet (root, or the first clipping ancestor in the
        // chain), the container's own geometry becomes the clip outright.
        const SRect childClipRect = ClipsChildren()
            ? (clipRect.IsValid() ? SRect::Intersect(m_Geometry, clipRect) : m_Geometry)
            : clipRect;

        ForEachChild([&](const Ref<Widget>& child)
        {
            child->CollectDrawCommands(batch, zCursor, rebuilt, childClipRect);
        });
    }

    void Widget::MarkLayoutDirty()
    {
        // Bump before the short-circuit: a change while already dirty must still be seen by
        // the Manager's frame gate (layout changes may move geometry -> the batch is stale).
        ++s_DirtyEpoch;

        if (m_LayoutDirty)
            return;

        m_LayoutDirty = true;
        m_MeasureDirty = true;

        if (const auto parent = m_Parent.lock())
            parent->MarkLayoutDirty();
    }

    void Widget::MarkRenderDirty()
    {
        m_RenderDirty = true;
        ++s_DirtyEpoch;
    }

    bool Widget::HitTestSelf(const glm::vec2& point) const
    {
        return m_Geometry.Contains(point);
    }

    void Widget::HandleMouseEnter()
    {
        m_Hovered = true;
        MarkRenderDirty();
        if (m_OnMouseEnterCallback) m_OnMouseEnterCallback();
    }

    void Widget::HandleMouseLeave()
    {
        m_Hovered = false;
        MarkRenderDirty();
        if (m_OnMouseLeaveCallback) m_OnMouseLeaveCallback();
    }

    SInputReply Widget::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        if (!m_OnMouseDownCallback && !m_OnClickCallback && !m_OnMouseUpCallback)
            return SInputReply::Unhandled();

        m_Pressed = true;
        MarkRenderDirty();
        if (m_OnMouseDownCallback) m_OnMouseDownCallback();
        return SInputReply::HandledAndCaptured();
    }

    SInputReply Widget::HandleMouseUp(const MouseButtonReleasedEvent& event)
    {
        if (m_Pressed && m_OnMouseUpCallback)
            m_OnMouseUpCallback();

        m_Pressed = false;
        MarkRenderDirty();
        return SInputReply::Handled();
    }

    void Widget::HandleFocus()
    {
        m_Focused = true;
        MarkRenderDirty();
        if (m_OnFocusCallback) m_OnFocusCallback();
    }

    void Widget::HandleLostFocus()
    {
        m_Focused = false;
        MarkRenderDirty();
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
            case EHorizontalAlignment::Fill:
                result.Position.x = availableSpace.Position.x;
                result.Size.x = availableSpace.Size.x;
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
            case EVerticalAlignment::Fill:
                result.Position.y = availableSpace.Position.y;
                result.Size.y = availableSpace.Size.y;
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
        MarkRenderDirty();
        return *m_ContentSlot;
    }

    void ContentWidget::ClearContent()
    {
        if (!m_ContentSlot) return;

        DetachChild(m_ContentSlot->GetWidget());
        m_ContentSlot.reset();
        MarkRenderDirty();
    }

    void ContentWidget::RemoveChild(const Ref<Widget>& child)
    {
        if (m_ContentSlot && m_ContentSlot->GetWidget() == child)
            ClearContent();
    }

    Ref<Widget> ContentWidget::GetChildAt(const size_t index) const
    {
        if (m_ContentSlot && index == 0)
            return m_ContentSlot->GetWidget();

        return nullptr;
    }
}
