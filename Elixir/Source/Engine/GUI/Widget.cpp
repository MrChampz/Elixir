#include "epch.h"
#include "Widget.h"

#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    /* Widget */

    Widget::Widget()
      : m_Style(GetDefaultStyles().GetWidgetStyle<SWidgetStyle>()) {}

    const glm::vec2& Widget::Measure(const glm::vec2& availableSize)
    {
        if (!m_MeasureDirty && m_LastMeasureConstraint == availableSize)
            return m_DesiredSize;

        // Border-box sizing: the outline is drawn INSET, in the outer band of this widget's
        // own geometry (see applyOutline in GUI.ps.hlsl), so - like a CSS border, unlike a
        // CSS outline - it has to be budgeted for here rather than left to bleed past
        // whatever ComputeDesiredSize reports. A leaf that wants a 10x10 content+padding box
        // with a 1px outline must actually occupy 12x12, or the outline eats into its own
        // content instead of wrapping around it. Subtracting first gives ComputeDesiredSize
        // the real content budget when this widget is itself content-constrained; adding
        // back after is a no-op with an UnconstrainedSize (infinity - 2 is still infinity) or
        // when there's no outline at all (the common case).
        const float outlineSpace = GetResolvedAppearance().Background.Outline.Thickness * 2.0f;
        const glm::vec2 innerAvailable = {
            std::max(0.0f, availableSize.x - outlineSpace),
            std::max(0.0f, availableSize.y - outlineSpace)
        };

        m_DesiredSize = ComputeDesiredSize(innerAvailable) + glm::vec2(outlineSpace, outlineSpace);
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

    void Widget::HitTest(
        const glm::vec2& point,
        std::vector<Ref<Widget>>& path,
        const SRect& clipRect
    )
    {
        // HitTestInvisible prunes this whole branch (neither this widget nor its children can
        // be hit); Hidden/Collapsed are not rendered/laid out, so neither should be clickable.
        if (m_Visibility == EVisibility::HitTestInvisible ||
            m_Visibility == EVisibility::Hidden ||
            m_Visibility == EVisibility::Collapsed)
            return;

        if (clipRect.IsValid() && !clipRect.Contains(point))
            return;

        const SRect childClipRect = ClipsChildren()
            ? (clipRect.IsValid() ? SRect::Intersect(m_Geometry, clipRect) : m_Geometry)
            : clipRect;

        // Children sit above their parent z (see CollectDrawCommands' pre-order zCursor):
        // test the topmost child first and recurse depth-first, so the first branch that
        // reports a hit wins.
        for (size_t i = GetChildCount(); i-- > 0;)
        {
            if (const Ref<Widget> child = GetChildAt(i))
            {
                const size_t sizeBefore = path.size();
                child->HitTest(point, path, childClipRect);

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
        ++s_DirtyEpoch;
    }

    void Widget::SetRenderOffset(const glm::vec2& offset)
    {
        if (m_RenderOffset == offset) return;
        m_RenderOffset = offset;
        ++s_DirtyEpoch;
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

    bool Widget::CanHandleMouseInput() const
    {
        return m_Enabled && (
            m_OnMouseEnterCallback ||
            m_OnMouseLeaveCallback ||
            m_OnMouseDownCallback ||
            m_OnMouseUpCallback ||
            m_OnClickCallback
        );
    }

    const SWidgetStyle& Widget::GetStyle() const
    {
        return m_Style;
    }

    void Widget::SetStyle(const SWidgetStyle& style)
    {
        m_Style = style;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    SBrush& Widget::GetMutableBackgroundBrush(const EStyleLayer layer)
    {
        return m_Style.Get(layer).Background;
    }

    void Widget::SetBackgroundColor(const EStyleLayer layer, const SColor& color)
    {
        GetMutableBackgroundBrush(layer).Color = color;
        MarkRenderDirty();
    }

    void Widget::SetBackgroundTexture(const EStyleLayer layer, const Ref<Texture2D>& texture)
    {
        GetMutableBackgroundBrush(layer).Texture = texture;
        MarkRenderDirty();
    }

    void Widget::ClearBackgroundTexture(const EStyleLayer layer)
    {
        GetMutableBackgroundBrush(layer).Texture.reset();
        MarkRenderDirty();
    }

    void Widget::SetBackgroundBorders(const EStyleLayer layer, const glm::vec4& borders)
    {
        GetMutableBackgroundBrush(layer).Borders = borders;
        MarkRenderDirty();
    }

    void Widget::SetCornerRadius(const EStyleLayer layer, const glm::vec4& radius)
    {
        GetMutableBackgroundBrush(layer).CornerRadius = radius;
        MarkRenderDirty();
    }

    void Widget::SetInsetShadow(const EStyleLayer layer, const glm::vec4& shadow)
    {
        GetMutableBackgroundBrush(layer).InsetShadow = shadow;
        MarkRenderDirty();
    }

    void Widget::SetInsetShadowOffset(const EStyleLayer layer, const glm::vec2& offset)
    {
        auto& shadow = GetMutableBackgroundBrush(layer).InsetShadow;
        shadow = { offset, shadow.z, shadow.w };
        MarkRenderDirty();
    }

    void Widget::SetInsetShadowBlur(const EStyleLayer layer, const float blur)
    {
        auto& shadow = GetMutableBackgroundBrush(layer).InsetShadow;
        shadow.z = blur;
        MarkRenderDirty();
    }

    void Widget::SetInsetShadowIntensity(const EStyleLayer layer, const float intensity)
    {
        auto& shadow = GetMutableBackgroundBrush(layer).InsetShadow;
        shadow.w = intensity;
        MarkRenderDirty();
    }

    void Widget::SetDropShadow(const EStyleLayer layer, const glm::vec4& shadow)
    {
        GetMutableBackgroundBrush(layer).DropShadow = shadow;
        MarkRenderDirty();
    }

    void Widget::SetDropShadowOffset(const EStyleLayer layer, const glm::vec2& offset)
    {
        auto& shadow = GetMutableBackgroundBrush(layer).DropShadow;
        shadow = { offset, shadow.z, shadow.w };
        MarkRenderDirty();
    }

    void Widget::SetDropShadowBlur(const EStyleLayer layer, const float blur)
    {
        GetMutableBackgroundBrush(layer).DropShadow.z = blur;
        MarkRenderDirty();
    }

    void Widget::SetDropShadowIntensity(const EStyleLayer layer, const float intensity)
    {
        GetMutableBackgroundBrush(layer).DropShadow.w = intensity;
        MarkRenderDirty();
    }

    void Widget::SetOutline(const EStyleLayer layer, const SOutline& outline)
    {
        GetMutableBackgroundBrush(layer).Outline = outline;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void Widget::SetOutlineColor(const EStyleLayer layer, const SColor& color)
    {
        GetMutableBackgroundBrush(layer).Outline.Color = color;
        MarkRenderDirty();
    }

    void Widget::SetOutlineThickness(const EStyleLayer layer, const float thickness)
    {
        GetMutableBackgroundBrush(layer).Outline.Thickness = thickness;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void Widget::SetFocusable(const bool focusable)
    {
        if (m_Focusable == focusable) return;

        // Purely a membership change in Manager::BuildFocusOrder's cached traversal, not a
        // layout or visual change - MarkLayoutDirty/MarkRenderDirty would both do more than
        // needed (and MarkRenderDirty alone would still be a lie: nothing about this widget's
        // own draw commands changed). Bumping s_DirtyEpoch directly is enough to invalidate
        // Manager's focus-order cache, which keys off the same epoch as everything else that
        // reuses it (see Manager::GetFocusOrder).
        m_Focusable = focusable;
        ++s_DirtyEpoch;
    }

    void Widget::SetEnabled(const bool enabled)
    {
        if (m_Enabled == enabled) return;

        m_Enabled = enabled;

        // Cancels a press in progress immediately, rather than waiting for the eventual
        // mouse-up to see m_Enabled == false: also fixes the pressed-looking visual without
        // waiting for that mouse-up.
        if (!m_Enabled)
            m_Pressed = false;

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
        const SRect& clipRect,
        const glm::vec2 inheritedOffset,
        const float inheritedOpacity
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

        const glm::vec2 renderOffset = inheritedOffset + m_RenderOffset;
        const float renderOpacity = inheritedOpacity * m_Opacity;

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
        batch.Append(m_CachedCommands, zCursor, clipRect, renderOffset, renderOpacity);
        zCursor += m_CachedCommands.LayerSpan();

        // A clipping container (e.g. ScrollBox) intersects its own bounds with whatever clip
        // it inherited and hands that down; everyone else just forwards the inherited clip
        // unchanged. With no inherited clip yet (root, or the first clipping ancestor in the
        // chain), the container's own geometry becomes the clip outright.
        SRect renderGeometry = m_Geometry;
        renderGeometry.Position += renderOffset;
        const SRect childClipRect = ClipsChildren()
            ? (clipRect.IsValid()
                ? SRect::Intersect(renderGeometry, clipRect)
                : renderGeometry)
            : clipRect;

        ForEachChild([&](const Ref<Widget>& child)
        {
            child->CollectDrawCommands(
                batch,
                zCursor,
                rebuilt,
                childClipRect,
                renderOffset,
                renderOpacity
            );
        });
    }

    EInteractionState Widget::GetInteractionState() const
    {
        auto states = EInteractionState::None;

        if (IsHovered())
            states |= EInteractionState::Hovered;

        if (IsPressed())
            states |= EInteractionState::Pressed;

        if (IsFocused())
            states |= EInteractionState::Focused;

        if (!IsEnabled())
            states |= EInteractionState::Disabled;

        return states;
    }

    const SAppearance& Widget::GetResolvedAppearance() const
    {
        return GetStyle().Resolve(GetInteractionState());
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
        if (!m_Enabled) return SInputReply::Unhandled();
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
        if (m_Enabled && m_OnClickCallback) m_OnClickCallback();
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
        Widget::Update(frameTime);
        if (m_ContentSlot && m_ContentSlot->GetWidget()->TakesSpace())
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
