#include "epch.h"
#include "ScrollBox.h"

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
        const glm::vec2 clamped = ClampScrollOffset(offset, m_Geometry.Size);
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
            const glm::vec2 contentConstraint = ContentMeasureConstraint(desired);
            const glm::vec2 contentSize = m_ContentSlot->GetWidget()->Measure(contentConstraint);
            const glm::vec2 contentViewport = CrossAxisSpace(desired);

            const bool verticalScrollbarVisible =
                m_ShowScrollbar &&
                m_ScrollAxis != EScrollAxis::Horizontal &&
                contentSize.y > contentViewport.y;
            const bool horizontalScrollbarVisible =
                m_ShowScrollbar &&
                m_ScrollAxis != EScrollAxis::Vertical &&
                contentSize.x > contentViewport.x;

            glm::vec2 contentSizeWithGutters = contentSize;
            if (verticalScrollbarVisible)
                contentSizeWithGutters.x += m_ScrollBarStyle.Thickness;
            if (horizontalScrollbarVisible)
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
            m_ContentSize = {};
            return;
        }

        const auto& content = m_ContentSlot->GetWidget();

        // The content keeps its DESIRED size along the scrolling axis/axes - that's what
        // there is to scroll through - but is capped to the viewport on the other axis,
        // same as a non-scrolling child would be.
        const glm::vec2 contentConstraint = ContentMeasureConstraint(allocatedSpace.Size);
        const glm::vec2 desired = content->Measure(contentConstraint);

        // Same gutter CrossAxisSpace reserves in ContentMeasureConstraint, applied to the
        // space content is actually ARRANGED into - measuring content against a narrower
        // width but then stretching it back out to the full viewport here would put it right
        // back under the scrollbar it was just measured to avoid.
        const glm::vec2 crossAxisSpace = CrossAxisSpace(allocatedSpace.Size);

        glm::vec2 contentSize = crossAxisSpace;
        if (m_ScrollAxis != EScrollAxis::Horizontal) contentSize.y = desired.y;
        if (m_ScrollAxis != EScrollAxis::Vertical)   contentSize.x = desired.x;

        const glm::vec2 previousContentSize = m_ContentSize;
        const glm::vec2 previousScrollOffset = m_ScrollOffset;

        m_ContentSize = contentSize;
        const glm::vec2 clampedOffset = ClampScrollOffset(previousScrollOffset, allocatedSpace.Size);
        m_ScrollOffset = clampedOffset;

        if (m_ContentSize != previousContentSize || m_ScrollOffset != previousScrollOffset)
            MarkRenderDirty(); // scrollbar thumb size and position changed

        const SRect contentRect = { allocatedSpace.Position - m_ScrollOffset, contentSize };
        content->ArrangeChildren(contentRect);
    }

    void ScrollBox::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        if (!m_ShowScrollbar) return;

        const glm::vec2 contentViewport = CrossAxisSpace(m_Geometry.Size);

        if (m_ScrollAxis != EScrollAxis::Horizontal && m_ContentSize.y > contentViewport.y)
            AddScrollbar(batch, zOrder, true);

        if (m_ScrollAxis != EScrollAxis::Vertical && m_ContentSize.x > contentViewport.x)
            AddScrollbar(batch, zOrder, false);
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

        const glm::vec2 clamped = ClampScrollOffset(m_ScrollOffset + delta, m_Geometry.Size);
        if (clamped == m_ScrollOffset)
            return SInputReply::Unhandled(); // at the edge; let an ancestor try.

        m_ScrollOffset = clamped;
        MarkLayoutDirty(); // reposition content
        MarkRenderDirty(); // thumb moved

        return SInputReply::Handled();
    }

    glm::vec2 ScrollBox::CrossAxisSpace(const glm::vec2& viewportSize) const
    {
        glm::vec2 space = viewportSize;
        if (!m_ShowScrollbar) return space;

        // Reserve a gutter for the scrollbar on whichever axis it actually occupies, so
        // content never has to be measured or arranged under it - same reasoning as an
        // outline budgeting its own space in Widget::Measure instead of bleeding into
        // whatever's next to it. A vertical scrollbar (shown whenever this axis isn't purely
        // Horizontal) is itself thickness-wide, eating into the content's width; a horizontal
        // one eats into its height.
        if (m_ScrollAxis != EScrollAxis::Horizontal)
            space.x = std::max(0.0f, space.x - m_ScrollBarStyle.Thickness);
        if (m_ScrollAxis != EScrollAxis::Vertical)
            space.y = std::max(0.0f, space.y - m_ScrollBarStyle.Thickness);

        return space;
    }

    glm::vec2 ScrollBox::ContentMeasureConstraint(const glm::vec2& viewportSize) const
    {
        glm::vec2 constraint = CrossAxisSpace(viewportSize);
        if (m_ScrollAxis != EScrollAxis::Horizontal) constraint.y = UnconstrainedSize;
        if (m_ScrollAxis != EScrollAxis::Vertical)   constraint.x = UnconstrainedSize;
        return constraint;
    }

    glm::vec2 ScrollBox::ClampScrollOffset(
        const glm::vec2& offset,
        const glm::vec2& viewportSize
    ) const
    {
        const glm::vec2 contentViewport = CrossAxisSpace(viewportSize);
        const glm::vec2 maxOffset = glm::max(m_ContentSize - contentViewport, glm::vec2(0.0f));
        return glm::clamp(offset, glm::vec2(0.0f), maxOffset);
    }

    void ScrollBox::AddScrollbar(RenderBatch& batch, const int zOrder, const bool vertical) const
    {
        const auto& appearance = m_ScrollBarStyle.Resolve(GetInteractionState());
        const float thickness = m_ScrollBarStyle.Thickness;
        const glm::vec2 contentViewport = CrossAxisSpace(m_Geometry.Size);

        if (vertical)
        {
            const SRect track = {
                { m_Geometry.Position.x + contentViewport.x, m_Geometry.Position.y },
                { thickness, contentViewport.y }
            };

            const float maxScroll = m_ContentSize.y - contentViewport.y;
            const float thumbHeight = std::min(
                track.Size.y,
                std::max(
                    track.Size.y * (contentViewport.y / m_ContentSize.y),
                    m_ScrollBarStyle.MinimumThumbLength
                )
            );
            const float scrollRatio = maxScroll > 0.0f ? m_ScrollOffset.y / maxScroll : 0.0f;

            const SRect thumb = {
                { track.Position.x, track.Position.y + scrollRatio * (track.Size.y - thumbHeight) },
                { thickness, thumbHeight }
            };

            batch.AddBrush(appearance.Track, track, zOrder);
            batch.AddBrush(appearance.Thumb, thumb, zOrder + 1);
        }
        else
        {
            const SRect track = {
                { m_Geometry.Position.x, m_Geometry.Position.y + contentViewport.y },
                { contentViewport.x, thickness }
            };

            const float maxScroll = m_ContentSize.x - contentViewport.x;
            const float thumbWidth = std::min(
                track.Size.x,
                std::max(
                    track.Size.x * (contentViewport.x / m_ContentSize.x),
                    m_ScrollBarStyle.MinimumThumbLength
                )
            );
            const float scrollRatio = maxScroll > 0.0f ? m_ScrollOffset.x / maxScroll : 0.0f;

            const SRect thumb = {
                { track.Position.x + scrollRatio * (track.Size.x - thumbWidth), track.Position.y },
                { thumbWidth, thickness }
            };

            batch.AddBrush(appearance.Track, track, zOrder);
            batch.AddBrush(appearance.Thumb, thumb, zOrder + 1);
        }
    }
}
