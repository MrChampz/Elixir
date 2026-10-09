#include "epch.h"
#include "ScrollBox.h"
#include <Engine/Input/InputCodes.h>

#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    ScrollBox::ScrollBox()
      : m_ScrollBarStyle(GetDefaultStyles().GetWidgetStyle<SScrollBarStyle>())
    {
    }

    void ScrollBox::SetStyle(const SScrollBarStyle& style)
    {
        m_ScrollBarStyle = style;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void ScrollBox::SetSize(const glm::vec2& size)
    {
        if (m_Size == size) return;
        m_Size = size;
        MarkLayoutDirty();
    }

    void ScrollBox::SetScrollAxis(EScrollAxis axis)
    {
        if (m_ScrollAxis == axis) return;
        m_ScrollAxis = axis;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void ScrollBox::SetScrollOffset(const glm::vec2& offset)
    {
        const SScrollbarVisibility visibility = ResolveScrollbarVisibility(m_Geometry.Size);
        const glm::vec2 clamped = ClampScrollOffset(offset, m_Geometry.Size, visibility);
        if (m_ScrollOffset == clamped) return;

        m_ScrollOffset = clamped;
        MarkLayoutDirty(); // reposition content
        MarkRenderDirty(); // thumb moved
    }

    void ScrollBox::SetShowScrollbar(bool show)
    {
        if (m_ShowScrollbar == show) return;
        m_ShowScrollbar = show;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void ScrollBox::SetScrollbarThickness(const float thickness)
    {
        if (m_ScrollBarStyle.Thickness == thickness) return;
        m_ScrollBarStyle.Thickness = thickness;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void ScrollBox::SetScrollbarColor(const SColor& color)
    {
        m_ScrollBarStyle.Normal.Thumb.Color = color;
        MarkRenderDirty();
    }

    glm::vec2 ScrollBox::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        glm::vec2 desired = glm::min(m_Size, availableSize);

        // Content can only shrink the reported size toward itself, never grow it past the
        // configured viewport size - a ScrollBox clips oversized content, it doesn't expand
        // to swallow it. Measure (not ComputeDesiredSize) is the public, cached entry point
        // every container is expected to call on a child.
        if (HasContent())
        {
            const SScrollbarVisibility visibility = ResolveScrollbarVisibility(desired);
            const glm::vec2 contentConstraint = ContentMeasureConstraint(desired, visibility);
            const glm::vec2 contentSize = m_ContentSlot->GetWidget()->Measure(contentConstraint);

            glm::vec2 contentSizeWithGutters = contentSize;
            if (visibility.Vertical)
                contentSizeWithGutters.x += m_ScrollBarStyle.Thickness;
            if (visibility.Horizontal)
                contentSizeWithGutters.y += m_ScrollBarStyle.Thickness;

            // Content is measured against the gutter-reduced cross axis. Include each
            // visible gutter in the desired size so a narrow scrolling child is not laid
            // out underneath its own scrollbar.
            desired = glm::min(desired, contentSizeWithGutters);
        }

        return desired;
    }

    void ScrollBox::LayoutChildren(const SRect& allocatedSpace)
    {
        if (!HasContent())
        {
            const bool visibilityChanged =
                m_ScrollbarVisibility.Vertical || m_ScrollbarVisibility.Horizontal;
            m_ContentSize = {};
            m_ScrollbarVisibility = {};
            if (visibilityChanged) MarkRenderDirty();
            return;
        }

        const auto& content = m_ContentSlot->GetWidget();
        const SScrollbarVisibility visibility = ResolveScrollbarVisibility(allocatedSpace.Size);

        // The content keeps its DESIRED size along the scrolling axis/axes - that's what
        // there is to scroll through - but is capped to the viewport on the other axis,
        // same as a non-scrolling child would be.
        const glm::vec2 contentConstraint = ContentMeasureConstraint(allocatedSpace.Size, visibility);
        const glm::vec2 desired = content->Measure(contentConstraint);

        // Same gutter CrossAxisSpace reserves in ContentMeasureConstraint, applied to the
        // space content is actually ARRANGED into - measuring content against a narrower
        // width but then stretching it back out to the full viewport here would put it right
        // back under the scrollbar it was just measured to avoid.
        const glm::vec2 crossAxisSpace = CrossAxisSpace(allocatedSpace.Size, visibility);

        glm::vec2 contentSize = crossAxisSpace;
        if (m_ScrollAxis != EScrollAxis::Horizontal) contentSize.y = desired.y;
        if (m_ScrollAxis != EScrollAxis::Vertical)   contentSize.x = desired.x;

        const glm::vec2 previousContentSize = m_ContentSize;
        const glm::vec2 previousScrollOffset = m_ScrollOffset;
        const SScrollbarVisibility previousVisibility = m_ScrollbarVisibility;

        m_ContentSize = contentSize;
        m_ScrollbarVisibility = visibility;
        const glm::vec2 clampedOffset = ClampScrollOffset(
            previousScrollOffset,
            allocatedSpace.Size,
            visibility
        );
        m_ScrollOffset = clampedOffset;

        if (m_ContentSize != previousContentSize || m_ScrollOffset != previousScrollOffset ||
            m_ScrollbarVisibility.Vertical != previousVisibility.Vertical ||
            m_ScrollbarVisibility.Horizontal != previousVisibility.Horizontal)
            MarkRenderDirty(); // scrollbar thumb size and position changed

        const SRect contentRect = { allocatedSpace.Position - m_ScrollOffset, contentSize };
        content->ArrangeChildren(contentRect);
    }

    void ScrollBox::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        if (!m_ShowScrollbar) return;

        if (m_ScrollbarVisibility.Vertical)
            AddScrollbar(batch, zOrder, true, m_ScrollbarVisibility);

        if (m_ScrollbarVisibility.Horizontal)
            AddScrollbar(batch, zOrder, false, m_ScrollbarVisibility);
    }

    SInputReply ScrollBox::HandleMouseScrolled(const MouseScrolledEvent& event)
    {
        glm::vec2 delta{};

        if (m_ScrollAxis != EScrollAxis::Horizontal)
            delta.y = -event.GetOffsetY() * SCROLL_SPEED;
        if (m_ScrollAxis != EScrollAxis::Vertical)
            delta.x = -event.GetOffsetX() * SCROLL_SPEED;

        if (delta == glm::vec2(0.0f))
            return SInputReply::Unhandled();

        const glm::vec2 clamped = ClampScrollOffset(
            m_ScrollOffset + delta,
            m_Geometry.Size,
            m_ScrollbarVisibility
        );
        if (clamped == m_ScrollOffset)
            return SInputReply::Unhandled(); // at the edge; let an ancestor try.

        m_ScrollOffset = clamped;
        MarkLayoutDirty(); // reposition content
        MarkRenderDirty(); // thumb moved

        return SInputReply::Handled();
    }

    ScrollBox::SScrollbarVisibility ScrollBox::ResolveScrollbarVisibility(
        const glm::vec2& viewportSize
    ) const
    {
        SScrollbarVisibility visibility;
        if (!m_ShowScrollbar || !HasContent()) return visibility;

        const auto& content = m_ContentSlot->GetWidget();
        while (true)
        {
            const glm::vec2 contentConstraint = ContentMeasureConstraint(viewportSize, visibility);
            const glm::vec2 contentSize = content->Measure(contentConstraint);
            const glm::vec2 contentViewport = CrossAxisSpace(viewportSize, visibility);

            SScrollbarVisibility required = visibility;
            if (m_ScrollAxis != EScrollAxis::Horizontal && contentSize.y > contentViewport.y)
                required.Vertical = true;
            if (m_ScrollAxis != EScrollAxis::Vertical && contentSize.x > contentViewport.x)
                required.Horizontal = true;

            if (required.Vertical == visibility.Vertical &&
                required.Horizontal == visibility.Horizontal)
                return visibility;

            visibility = required;
        }
    }

    glm::vec2 ScrollBox::CrossAxisSpace(
        const glm::vec2& viewportSize,
        const SScrollbarVisibility& visibility
    ) const
    {
        glm::vec2 space = viewportSize;

        if (visibility.Vertical)
            space.x = std::max(0.0f, space.x - m_ScrollBarStyle.Thickness);
        if (visibility.Horizontal)
            space.y = std::max(0.0f, space.y - m_ScrollBarStyle.Thickness);

        return space;
    }

    glm::vec2 ScrollBox::ContentMeasureConstraint(
        const glm::vec2& viewportSize,
        const SScrollbarVisibility& visibility
    ) const
    {
        glm::vec2 constraint = CrossAxisSpace(viewportSize, visibility);
        if (m_ScrollAxis != EScrollAxis::Horizontal) constraint.y = UnconstrainedSize;
        if (m_ScrollAxis != EScrollAxis::Vertical)   constraint.x = UnconstrainedSize;
        return constraint;
    }

    glm::vec2 ScrollBox::ClampScrollOffset(
        const glm::vec2& offset,
        const glm::vec2& viewportSize,
        const SScrollbarVisibility& visibility
    ) const
    {
        const glm::vec2 contentViewport = CrossAxisSpace(viewportSize, visibility);
        const glm::vec2 maxOffset = glm::max(m_ContentSize - contentViewport, glm::vec2(0.0f));
        return glm::clamp(offset, glm::vec2(0.0f), maxOffset);
    }

    ScrollBox::SScrollbarGeometry ScrollBox::GetScrollbarGeometry(const bool vertical) const
    {
        const int axis = vertical ? 1 : 0;
        const float thickness = m_ScrollBarStyle.Thickness;
        const glm::vec2 viewport = CrossAxisSpace(m_Geometry.Size, m_ScrollbarVisibility);
        SScrollbarGeometry geometry{};
        geometry.Track = vertical
            ? SRect{{m_Geometry.Position.x + viewport.x, m_Geometry.Position.y}, {thickness, viewport.y}}
            : SRect{{m_Geometry.Position.x, m_Geometry.Position.y + viewport.y}, {viewport.x, thickness}};
        geometry.MaxScroll = std::max(0.0f, m_ContentSize[axis] - viewport[axis]);
        const float length = geometry.Track.Size[axis];
        const float thumbLength = m_ContentSize[axis] > 0
            ? std::min(length, std::max(length * viewport[axis] / m_ContentSize[axis], m_ScrollBarStyle.MinimumThumbLength))
            : length;
        geometry.Thumb = geometry.Track;
        geometry.Thumb.Size[axis] = thumbLength;
        if (geometry.MaxScroll > 0)
            geometry.Thumb.Position[axis] += m_ScrollOffset[axis] / geometry.MaxScroll * (length - thumbLength);
        return geometry;
    }

    SInputReply ScrollBox::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        if (!IsEnabled() || !m_ShowScrollbar || event.GetMouseButton() != EE_MOUSE_BUTTON_LEFT)
            return Widget::HandleMouseDown(event);
        const glm::vec2 position{event.GetX(), event.GetY()};
        for (const int axis : {1, 0})
        {
            if (!(axis == 1 ? m_ScrollbarVisibility.Vertical : m_ScrollbarVisibility.Horizontal)) continue;
            const auto geometry = GetScrollbarGeometry(axis == 1);
            if (!geometry.Track.Contains(position)) continue;
            m_DragAxis = axis;
            m_DragGrabOffset = geometry.Thumb.Contains(position)
                ? position[axis] - geometry.Thumb.Position[axis] : geometry.Thumb.Size[axis] * 0.5f;
            m_Pressed = true;
            DragScrollbar(position);
            MarkRenderDirty();
            return SInputReply::HandledAndCaptured();
        }
        return Widget::HandleMouseDown(event);
    }

    void ScrollBox::DragScrollbar(const glm::vec2& position)
    {
        const auto geometry = GetScrollbarGeometry(m_DragAxis == 1);
        const float travel = geometry.Track.Size[m_DragAxis] - geometry.Thumb.Size[m_DragAxis];
        if (travel <= 0) return;
        auto offset = m_ScrollOffset;
        offset[m_DragAxis] = std::clamp(
            (position[m_DragAxis] - geometry.Track.Position[m_DragAxis] - m_DragGrabOffset) / travel,
            0.0f, 1.0f) * geometry.MaxScroll;
        SetScrollOffset(offset);
    }

    SInputReply ScrollBox::HandleMouseMove(const MouseMovedEvent& event)
    {
        if (m_DragAxis < 0) return Widget::HandleMouseMove(event);
        if (IsEnabled() && m_ShowScrollbar) DragScrollbar({event.GetX(), event.GetY()});
        return SInputReply::Handled();
    }

    SInputReply ScrollBox::HandleMouseUp(const MouseButtonReleasedEvent& event)
    {
        if (m_DragAxis < 0 || event.GetMouseButton() != EE_MOUSE_BUTTON_LEFT)
            return Widget::HandleMouseUp(event);
        m_DragAxis = -1;
        m_Pressed = false;
        MarkRenderDirty();
        return SInputReply::Handled();
    }

    void ScrollBox::AddScrollbar(RenderBatch& batch, const int zOrder, const bool vertical,
        const SScrollbarVisibility&) const
    {
        const auto& appearance = m_ScrollBarStyle.Resolve(GetInteractionState());
        const auto geometry = GetScrollbarGeometry(vertical);
        batch.AddBrush(appearance.Track, geometry.Track, zOrder);
        batch.AddBrush(appearance.Thumb, geometry.Thumb, zOrder + 1);
    }
}
