#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    enum class EScrollAxis : uint8_t
    {
        Vertical, Horizontal, Both
    };

    /** @brief Visual data for a scrollbar in one interaction state. */
    struct SScrollBarAppearance
    {
        SBrush Track;
        SBrush Thumb;
    };

    /** @brief Complete visual and sizing style for a scrollbar. */
    struct SScrollBarStyle final : SStyle, TStateStyles<SScrollBarAppearance>
    {
        float Thickness = 8.0f;
        float MinimumThumbLength = 8.0f;
    };

    class ELIXIR_API ScrollBox : public ContentWidget
    {
    public:
        ScrollBox();

        /**
         * @brief Replace this scroll box's complete scrollbar style.
         * @param style Style to copy.
         */
        void SetStyle(const SScrollBarStyle& style);

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

        float GetScrollbarThickness() const { return m_ScrollBarStyle.Thickness; }
        void SetScrollbarThickness(float thickness);

        SColor GetScrollbarColor() const { return m_ScrollBarStyle.Normal.Thumb.Color; }
        void SetScrollbarColor(const SColor& color);

        bool CanHandleMouseInput() const override { return IsEnabled(); }

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;

        bool ClipsChildren() const override { return true; }

        void LayoutChildren(const SRect& allocatedSpace) override;
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        SInputReply HandleMouseScrolled(const MouseScrolledEvent& event) override;

    private:
        struct SScrollbarVisibility
        {
            bool Vertical = false;
            bool Horizontal = false;
        };

        SScrollbarVisibility ResolveScrollbarVisibility(const glm::vec2& viewportSize) const;

        // viewportSize with the gutters for the scrollbars that are actually visible
        // subtracted. Shared by measurement and arrangement so content never sits under a
        // rendered scrollbar, without reserving space for one that is not needed.
        glm::vec2 CrossAxisSpace(
            const glm::vec2& viewportSize,
            const SScrollbarVisibility& visibility
        ) const;

        // Constraint handed to the content's Measure() call: UnconstrainedSize on every axis
        // this ScrollBox scrolls (so content reports its full natural size to scroll
        // through), CrossAxisSpace's result on the axis it doesn't (content is capped to the
        // viewport remaining after visible gutters, same as a non-scrolling child would be).
        glm::vec2 ContentMeasureConstraint(
            const glm::vec2& viewportSize,
            const SScrollbarVisibility& visibility
        ) const;

        glm::vec2 ClampScrollOffset(
            const glm::vec2& offset,
            const glm::vec2& viewportSize,
            const SScrollbarVisibility& visibility
        ) const;
        void AddScrollbar(
            RenderBatch& batch,
            int zOrder,
            bool vertical,
            const SScrollbarVisibility& visibility
        ) const;

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

        SScrollbarVisibility m_ScrollbarVisibility;

        bool m_ShowScrollbar = true;
        SScrollBarStyle m_ScrollBarStyle;
    };
}
