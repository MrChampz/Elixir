#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Event/KeyEvent.h>
#include <Engine/Event/MouseEvent.h>
#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Renderer/RenderBatch.h>

namespace Elixir::GUI
{
    class Manager;
    class ContentSlot;

    class ELIXIR_API Widget
    {
        friend class Manager;
        friend class Slot;
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
            if (!m_LayoutDirty && m_LastArrangedSpace == allocatedSpace)
                return;

            m_Geometry = allocatedSpace;
            m_LastArrangedSpace = allocatedSpace;
            m_LayoutDirty = false;
        }

        /**
         * Mark this widget's layout as dirty and propagate the mark to ancestors.
         * A dirty widget (and any ancestor whose layout depends on it) is re-arranged
         * on the next frame; clean subtrees are skipped by ArrangeChildren.
         */
        void MarkLayoutDirty()
        {
            if (m_LayoutDirty) return;
            m_LayoutDirty = true;
            if (m_Parent) m_Parent->MarkLayoutDirty();
        }

        bool IsLayoutDirty() const { return m_LayoutDirty; }

        /**
         * Mark this widget's visual output as dirty (colour, opacity, shadows, outline, ...).
         * Purely visual changes do not affect layout, so this does NOT touch the layout flag
         * nor propagate to ancestors — each widget owns its own draw commands.
         */
        void MarkRenderDirty() { m_RenderDirty = true; }

        bool IsRenderDirty() const { return m_RenderDirty; }
        void ClearRenderDirty() { m_RenderDirty = false; }

        /**
         * Invoke the visitor for each direct child of this widget.
         * Container widgets override this to expose their children; leaf widgets keep the
         * default no-op. Lets callers walk the widget tree without knowing concrete types.
         * @param visitor callback invoked once per child widget.
         */
        virtual void VisitChildren(const std::function<void(const Ref<Widget>&)>& visitor) const {}

        /**
         * Get the final computed geometry.
         * @return rect representing the widget geometry.
         */
        SRect GetGeometry() const { return m_Geometry; }

        glm::vec2 GetDesiredSize() const { return m_DesiredSize; }

        /* Callbacks */

        void OnFocus(const std::function<void()>& callback) { m_OnFocusCallback = callback; }
        void OnLostFocus(const std::function<void()>& callback) { m_OnLostFocusCallback = callback; }
        void OnClick(const std::function<void()>& callback) { m_OnClickCallback = callback; }
        void OnMouseEnter(const std::function<void()>& callback) { m_OnMouseEnterCallback = callback; }
        void OnMouseLeave(const std::function<void()>& callback) { m_OnMouseLeaveCallback = callback; }
        void OnMouseDown(const std::function<void()>& callback) { m_OnMouseDownCallback = callback; }
        void OnMouseUp(const std::function<void()>& callback) { m_OnMouseUpCallback = callback; }

        float GetOpacity() const { return m_Opacity; }
        void SetOpacity(const float opacity) { m_Opacity = opacity; MarkRenderDirty(); }

        EVisibility GetVisibility() const { return m_Visibility; }
        void SetVisibility(const EVisibility visibility) { m_Visibility = visibility; MarkLayoutDirty(); }
        bool IsVisible() const;

        glm::vec4 GetInsetShadow() const { return m_InsetShadow; }
        glm::vec4 GetDropShadow() const { return m_DropShadow; }

        /**
         * Set the inset shadow parameters.
         * @param shadow Shadow offset (x, y), blur (z) and intensity (w).
         */
        void SetInsetShadow(const glm::vec4& shadow) { m_InsetShadow = shadow; MarkRenderDirty(); }

        void SetInsetShadowOffset(const glm::vec2& offset) { m_InsetShadow.x = offset.x; m_InsetShadow.y = offset.y; MarkRenderDirty(); }
        void SetInsetShadowBlur(const float blur) { m_InsetShadow.z = blur; MarkRenderDirty(); }
        void SetInsetShadowIntensity(const float intensity) { m_InsetShadow.w = intensity; MarkRenderDirty(); }

        /**
         * Set the drop shadow parameters.
         * @param shadow Shadow offset (x, y), blur (z) and intensity (w).
         */
        void SetDropShadow(const glm::vec4& shadow) { m_DropShadow = shadow; MarkRenderDirty(); }

        void SetDropShadowOffset(const glm::vec2& offset) { m_DropShadow.x = offset.x; m_DropShadow.y = offset.y; MarkRenderDirty(); }
        void SetDropShadowBlur(const float blur) { m_DropShadow.z = blur; MarkRenderDirty(); }
        void SetDropShadowIntensity(const float intensity) { m_DropShadow.w = intensity; MarkRenderDirty(); }

        SOutline GetOutline() const { return m_Outline; }
        void SetOutline(const SOutline& outline) { m_Outline = outline; MarkRenderDirty(); }
        void SetOutlineColor(const SColor& color) { m_Outline.Color = color; MarkRenderDirty(); }
        void SetOutlineThickness(const float thickness) { m_Outline.Thickness = thickness; MarkRenderDirty(); }

        bool IsHovered() const { return m_Hovered; }
        bool IsPressed() const { return m_Pressed; }
        bool IsFocused() const { return m_Focused; }

      protected:
        virtual void HandleMouseEnter();
        virtual void HandleMouseLeave();
        virtual void HandleMouseDown(const MouseButtonPressedEvent& event);
        virtual void HandleMouseUp(const MouseButtonReleasedEvent& event);
        virtual void HandleMouseMove(const MouseMovedEvent&  event) {}
        virtual void HandleKeyPressed(const KeyPressedEvent& event) {}
        virtual void HandleKeyTyped(const KeyTypedEvent& event) {}
        virtual void HandleFocus();
        virtual void HandleLostFocus();
        virtual void HandleClick();

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

        SRect m_Geometry{};
        glm::vec2 m_DesiredSize{};

        // Layout dirty tracking. A widget starts dirty so the first frame always lays out.
        // m_LastArrangedSpace records the space received on the previous arrange, so a clean
        // widget still re-arranges when its parent hands it a different rectangle.
        Widget* m_Parent = nullptr;
        bool m_LayoutDirty = true;
        SRect m_LastArrangedSpace{};

        // Visual dirty flag (colour/opacity/shadow/...). Independent from layout; consumed by
        // the render-command cache. Starts dirty so the first frame generates commands.
        bool m_RenderDirty = true;

        float m_Opacity = 1.0f;

        EVisibility m_Visibility = EVisibility::Visible;

        glm::vec4 m_InsetShadow = {};
        glm::vec4 m_DropShadow = {};

        SOutline m_Outline = {};

        bool m_Hovered = false;
        bool m_Pressed = false;
        bool m_Focused = false;
        std::function<void()> m_OnMouseEnterCallback;
        std::function<void()> m_OnMouseLeaveCallback;
        std::function<void()> m_OnMouseDownCallback;
        std::function<void()> m_OnMouseUpCallback;
        std::function<void()> m_OnFocusCallback;
        std::function<void()> m_OnLostFocusCallback;
        std::function<void()> m_OnClickCallback;
    };

    class ELIXIR_API ContentWidget : public Widget
    {
      public:
        void Update(Timestep frameTime) override;

        void VisitChildren(const std::function<void(const Ref<Widget>&)>& visitor) const override;

        ContentSlot& SetContent(const Ref<Widget>& widget);

        Ref<ContentSlot> GetContentSlot() const { return m_ContentSlot; }
        void SetContentSlot(const Ref<ContentSlot>& slot) { m_ContentSlot = slot; }

        bool HasContent() const { return m_ContentSlot != nullptr; }

      protected:
        Ref<ContentSlot> m_ContentSlot;
    };
}