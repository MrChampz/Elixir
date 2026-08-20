#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    enum class EScrollAxis : uint8_t
    {
        Vertical, Horizontal, Both
    };

    class ELIXIR_API ScrollBox : public ContentWidget
    {
    public:
        ScrollBox();

        /**
         * @brief Set the viewport size this ScrollBox asks for.
         *
         * Unlike most containers, a ScrollBox never grows past this to fit its content -
         * that would defeat the point of scrolling. Content smaller than this still shrinks
         * the reported desired size, same as any other widget (see ComputeDesiredSize).
         * Distinct from the base Widget's m_DesiredSize, which the Measure() cache owns and
         * overwrites every call - this is the configured input to that computation, not its
         * cached output.
         *
         * @param size The viewport size.
         */
        void SetSize(const glm::vec2& size);

        EScrollAxis GetScrollAxis() const { return m_ScrollAxis; }
        void SetScrollAxis(EScrollAxis axis);

        glm::vec2 GetScrollOffset() const { return m_ScrollOffset; }
        void SetScrollOffset(const glm::vec2& offset);

        bool IsShowingScrollbar() const { return m_ShowScrollbar; }
        void SetShowScrollbar(bool show);

        float GetScrollbarThickness() const { return m_ScrollbarThickness; }
        void SetScrollbarThickness(float thickness);

        SColor GetScrollbarColor() const { return m_ScrollbarColor; }
        void SetScrollbarColor(const SColor& color);

    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;

        bool ClipsChildren() const override { return true; }

        void LayoutChildren(const SRect& allocatedSpace) override;
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        SInputReply HandleMouseScrolled(const MouseScrolledEvent& event) override;

    private:
        // viewportSize with the scrollbar's own gutter subtracted from whichever axis it
        // actually occupies (a no-op axis, or the whole thing, when m_ShowScrollbar is
        // false). Shared by ContentMeasureConstraint and LayoutChildren so content is
        // consistently measured AND arranged narrower than the scrollbar, never under it.
        glm::vec2 CrossAxisSpace(const glm::vec2& viewportSize) const;

        // Constraint handed to the content's Measure() call: UnconstrainedSize on every axis
        // this ScrollBox scrolls (so content reports its full natural size to scroll
        // through), CrossAxisSpace's result on the axis it doesn't (content is capped to the
        // gutter-reserved viewport there, same as a non-scrolling child would be).
        glm::vec2 ContentMeasureConstraint(const glm::vec2& viewportSize) const;

        glm::vec2 ClampScrollOffset(const glm::vec2& offset, const glm::vec2& viewportSize) const;
        void AddScrollbar(RenderBatch& batch, int zOrder, bool vertical) const;

        static constexpr float SCROLL_SPEED = 40.0f;

        EScrollAxis m_ScrollAxis = EScrollAxis::Vertical;
        glm::vec2 m_ScrollOffset{};

        // Configured viewport size; ComputeDesiredSize never returns more than this on
        // either axis. A reasonable non-zero default.
        glm::vec2 m_Size{ 200.0f, 200.0f };

        // Content's arranged size (desired size along the scrolling axis/axes, capped to
        // the viewport on the other axis). Recomputed by LayoutChildren; used to clamp
        // m_ScrollOffset and to size/position the scrollbar thumb.
        glm::vec2 m_ContentSize{};

        bool m_ShowScrollbar = true;
        float m_ScrollbarThickness = 8.0f;
        SColor m_ScrollbarColor{ 1.0f, 1.0f, 1.0f, 0.35f };
    };
}
