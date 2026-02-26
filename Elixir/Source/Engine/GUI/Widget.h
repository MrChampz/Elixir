#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Renderer/RenderBatch.h>

namespace Elixir::GUI
{
    class Manager;
    class ContentSlot;

    class ELIXIR_API Widget
    {
        friend class Manager;
      public:
        virtual ~Widget() = default;

        /**
         * Handles the update logic for the widget.
         * @param frameTime time since the last tick.
         */
        virtual void Update(Timestep frameTime) {}

        /**
         * Generate the draw commands for this widget.
         * @param batch batch to add draw commands to.
         * @param zOrder z-order for layering.
         */
        virtual void GenerateDrawCommands(RenderBatch& batch, int zOrder = 0) = 0;

        /**
         * Compute how much space this widget wants.
         * @return a 2d vector representing width and height.
         */
        virtual glm::vec2 ComputeDesiredSize() = 0;

        /**
         * Arrange this widget in the given space.
         * @param allocatedSpace the space available for this widget.
         */
        virtual void ArrangeChildren(const SRect& allocatedSpace)
        {
            m_Geometry = allocatedSpace;
        }

        /**
         * Get the final computed geometry.
         * @return rect representing the widget geometry.
         */
        SRect GetGeometry() const { return m_Geometry; }

        /* Callbacks */

        void OnClick(const std::function<void()>& callback) { m_OnClickCallback = callback; }
        void OnMouseEnter(const std::function<void()>& callback) { m_OnMouseEnterCallback = callback; }
        void OnMouseLeave(const std::function<void()>& callback) { m_OnMouseLeaveCallback = callback; }
        void OnMouseDown(const std::function<void()>& callback) { m_OnMouseDownCallback = callback; }
        void OnMouseUp(const std::function<void()>& callback) { m_OnMouseUpCallback = callback; }

        float GetOpacity() const { return m_Opacity; }
        void SetOpacity(const float opacity) { m_Opacity = opacity; }

        EVisibility GetVisibility() const { return m_Visibility; }
        void SetVisibility(const EVisibility visibility) { m_Visibility = visibility; }
        bool IsVisible() const;

        glm::vec4 GetInsetShadow() const { return m_InsetShadow; }
        glm::vec4 GetDropShadow() const { return m_DropShadow; }

        /**
         * Set the inset shadow parameters.
         * @param shadow Shadow offset (x, y), blur (z) and intensity (w).
         */
        void SetInsetShadow(const glm::vec4& shadow) { m_InsetShadow = shadow; }

        void SetInsetShadowOffset(const glm::vec2& offset) { m_InsetShadow.x = offset.x; m_InsetShadow.y = offset.y; }
        void SetInsetShadowBlur(const float blur) { m_InsetShadow.z = blur; }
        void SetInsetShadowIntensity(const float intensity) { m_InsetShadow.w = intensity; }

        /**
         * Set the drop shadow parameters.
         * @param shadow Shadow offset (x, y), blur (z) and intensity (w).
         */
        void SetDropShadow(const glm::vec4& shadow) { m_DropShadow = shadow; }

        void SetDropShadowOffset(const glm::vec2& offset) { m_DropShadow.x = offset.x; m_DropShadow.y = offset.y; }
        void SetDropShadowBlur(const float blur) { m_DropShadow.z = blur; }
        void SetDropShadowIntensity(const float intensity) { m_DropShadow.w = intensity; }

        SOutline GetOutline() const { return m_Outline; }
        void SetOutline(const SOutline& outline) { m_Outline = outline; }
        void SetOutlineColor(const SColor& color) { m_Outline.Color = color; }
        void SetOutlineThickness(const float thickness) { m_Outline.Thickness = thickness; }

        bool IsHovered() const { return m_IsHovered; }
        bool IsPressed() const { return m_IsPressed; }

      protected:
        /**
         * Apply padding  to a rectangle delimited by the available space.
         * @param availableSpace available space, the padding will be applied to.
         * @param padding padding to be applied.
         * @return a new rect, inside availableSpace with padding applied.
         */
        static SRect ApplyPadding(const SRect& availableSpace, const SPadding& padding);

        /**
         * Apply margin to a rectangle delimited by the available space.
         * @param availableSpace available space, the margin will be applied to.
         * @param margin margin to be applied.
         * @return a new rect, inside availableSpace with margin applied.
         */
        static SRect ApplyMargin(const SRect& availableSpace, const SMargin& margin);

        /**
         * Helper method: Apply alignment to a rectangle within available space.
         * @param childSize
         * @param availableSpace
         * @param hAlignment horizontal alignment
         * @param vAlignment vertical alignment
         * @param margin margin
         * @return
         */
        static SRect AlignChild(
            const glm::vec2& childSize,
            const SRect& availableSpace,
            EHorizontalAlignment hAlignment,
            EVerticalAlignment vAlignment,
            const SMargin& margin
        );

        /**
         * Helper method: Apply horizontal alignment to a rectangle within available space.
         * @param childSize child widget size (width, height)
         * @param availableSpace available space
         * @param alignment horizontal alignment
         * @return a rect aligned.
         */
        static SRect AlignHorizontally(
            const glm::vec2& childSize,
            const SRect& availableSpace,
            EHorizontalAlignment alignment
        );

        /**
         * Helper method: Apply vertical alignment to a rectangle within available space.
         * @param childSize child widget size (width, height)
         * @param availableSpace available space
         * @param alignment vertical alignment
         * @return a rect aligned.
         */
        static SRect AlignVertically(
            const glm::vec2& childSize,
            const SRect& availableSpace,
            EVerticalAlignment alignment
        );

        virtual void HandleMouseEnter();
        virtual void HandleMouseLeave();
        virtual void HandleMouseDown();
        virtual void HandleMouseUp();
        virtual void HandleClick();

        SRect m_Geometry{};
        glm::vec2 m_DesiredSize{};

        float m_Opacity = 1.0f;

        EVisibility m_Visibility = EVisibility::Visible;

        glm::vec4 m_InsetShadow = {};
        glm::vec4 m_DropShadow = {};

        SOutline m_Outline = {};

        bool m_IsHovered = false;
        bool m_IsPressed = false;
        std::function<void()> m_OnMouseEnterCallback;
        std::function<void()> m_OnMouseLeaveCallback;
        std::function<void()> m_OnMouseDownCallback;
        std::function<void()> m_OnMouseUpCallback;
        std::function<void()> m_OnClickCallback;
    };

    class ELIXIR_API ContentWidget : public Widget
    {
      public:
        void Update(Timestep frameTime) override;

        ContentSlot& SetContent(const Ref<Widget>& widget)
        {
            m_ContentSlot = CreateRef<ContentSlot>(widget);
            return *m_ContentSlot;
        }

        Ref<ContentSlot> GetContentSlot() const { return m_ContentSlot; }
        void SetContentSlot(const Ref<ContentSlot>& slot) { m_ContentSlot = slot; }

        bool HasContent() const { return m_ContentSlot != nullptr; }

      protected:
        Ref<ContentSlot> m_ContentSlot;
    };
}