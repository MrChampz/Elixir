#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Event/KeyEvent.h>
#include <Engine/Event/MouseEvent.h>
#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    class Manager;

    class ELIXIR_API Widget : public std::enable_shared_from_this<Widget>
    {
        friend class Manager;
        friend class Slot;
        friend class ContentSlot;
        friend class LayoutSlot;
        friend class CanvasSlot;
      public:
        virtual ~Widget() = default;

        /**
         * Handles the update logic for the widget.
         * @param frameTime time since the last tick.
         */
        virtual void Update(Timestep frameTime) {}

        /**
         * Compute how much space this widget wants.
         * @return a 2d vector representing width and height.
         */
        virtual glm::vec2 ComputeDesiredSize() = 0;

        /**
         * Arrange this widget in the given space. Short-circuits when the layout is clean and
         * the allocated space is unchanged; otherwise updates geometry and delegates the actual
         * layout to LayoutChildren before clearing the dirty flag. This is the template
         * method: it is non-virtual, so subclasses override LayoutChildren instead.
         * @param allocatedSpace the space available for this widget.
         */
        void ArrangeChildren(const SRect& allocatedSpace);

        /**
         * Get this widget's parent, or nullptr if it has none (or the parent was destroyed).
         * @return a Ref to the parent, kept alive for the duration of the call.
         */
        Ref<Widget> GetParent() const { return m_Parent.lock(); }

        /**
         * Get the final computed geometry.
         * @return rect representing the widget geometry.
         */
        SRect GetGeometry() const { return m_Geometry; }

        glm::vec2 GetDesiredSize() const { return m_DesiredSize; }

        bool IsLayoutDirty() const { return m_LayoutDirty; }
        bool IsRenderDirty() const { return m_RenderDirty; }

        /* Callbacks */

        void OnFocus(const std::function<void()>& callback) { m_OnFocusCallback = callback; }
        void OnLostFocus(const std::function<void()>& callback) { m_OnLostFocusCallback = callback; }
        void OnClick(const std::function<void()>& callback) { m_OnClickCallback = callback; }
        void OnMouseEnter(const std::function<void()>& callback) { m_OnMouseEnterCallback = callback; }
        void OnMouseLeave(const std::function<void()>& callback) { m_OnMouseLeaveCallback = callback; }
        void OnMouseDown(const std::function<void()>& callback) { m_OnMouseDownCallback = callback; }
        void OnMouseUp(const std::function<void()>& callback) { m_OnMouseUpCallback = callback; }

        float GetOpacity() const { return m_Opacity; }
        void SetOpacity(float opacity);

        EVisibility GetVisibility() const { return m_Visibility; }
        void SetVisibility(EVisibility visibility);
        bool IsVisible() const;

        glm::vec4 GetInsetShadow() const { return m_InsetShadow; }
        glm::vec4 GetDropShadow() const { return m_DropShadow; }

        /**
         * Set the inset shadow parameters.
         * @param shadow Shadow offset (x, y), blur (z) and intensity (w).
         */
        void SetInsetShadow(const glm::vec4& shadow);
        void SetInsetShadowOffset(const glm::vec2& offset);
        void SetInsetShadowBlur(float blur);
        void SetInsetShadowIntensity(float intensity);

        /**
         * Set the drop shadow parameters.
         * @param shadow Shadow offset (x, y), blur (z) and intensity (w).
         */
        void SetDropShadow(const glm::vec4& shadow);
        void SetDropShadowOffset(const glm::vec2& offset);
        void SetDropShadowBlur(float blur);
        void SetDropShadowIntensity(float intensity);

        SOutline GetOutline() const { return m_Outline; }
        void SetOutline(const SOutline& outline);
        void SetOutlineColor(const SColor& color);
        void SetOutlineThickness(float thickness);

        bool IsHovered() const { return m_Hovered; }
        bool IsPressed() const { return m_Pressed; }
        bool IsFocused() const { return m_Focused; }

      protected:
        /**
         * Register a widget as a child of this one: sets the child's parent back-pointer
         * (used for dirty propagation) and marks this widget's layout dirty, since gaining
         * a child changes layout. Call from container AddChild / SetContent methods.
         *
         * @param child the widget being attached to this one.
         */
        void AttachChild(const Ref<Widget>& child);

        /**
         * Unregister a widget as a child of this one: clears the child's parent back-pointer
         * so its dirty propagation no longer reaches this widget, and marks this widget's
         * layout dirty, since losing a child changes layout. Call when detaching or
         * replacing a child (e.g. from RemoveChild / SetContent).
         *
         * Only clears the link if the child is still parented to this widget.
         *
         * @param child the widget being detached to this one.
         */
        void DetachChild(const Ref<Widget>& child);

        virtual void RemoveChild(const Ref<Widget>& child) {}
        virtual void ForEachChild(const std::function<void(const Ref<Widget>&)>& fn) const {}

        /**
         * Position this widget's children within its (already updated) geometry. Container
         * widgets override this to lay out their children; leaf widgets keep the default no-op.
         * Invoked by ArrangeChildren only when a re-arrangement is actually needed, so the
         * dirty check, geometry update and dirty-flag reset are handled for you.
         * @param allocatedSpace the space allocated to this widget.
         */
        virtual void LayoutChildren(const SRect& allocatedSpace) {}

        /**
         * Walk this subtree appending each widget's cached draw commands to the batch,
         * regenerating only the caches of render-dirty widgets and reusing the rest.
         *
         * Threads a monotonic layer cursor in pre-order: a widget's own commands occupy
         * [zCursor, zCursor + LayerSpan()), children stack above, and the next sibling
         * starts above this widget's whole subtree — so sibling subtrees never overlap
         * in z.
         *
         * @param batch destination batch.
         * @param zCursor running layer index; advanced past everything this subtree.
         * @param rebuilt set to true if any widget's command cache was regenerated.
         */
        void CollectDrawCommands(RenderBatch& batch, int& zCursor, bool& rebuilt);

        /**
         * Build the draw commands for THIS widget only (no children). Containers emit their
         * own visuals (background, text, ...); children are walked separately by
         * CollectDrawCommands. Default is a no-op for widgets with nothing of their own to draw.
         * @param batch batch to add draw commands to.
         * @param zOrder z-order for layering, relative to this widget.
         */
        virtual void BuildDrawCommands(RenderBatch& batch, int zOrder) {}

        /**
         * Mark this widget's layout as dirty and propagate the mark to ancestors.
         * A dirty widget (and any ancestor whose layout depends on it) is re-arranged
         * on the next frame; clean subtrees are skipped by ArrangeChildren.
         */
        void MarkLayoutDirty();

        /**
         * Mark this widget's visual output as dirty (color, opacity, shadows, outline, ...).
         * Purely visual changes do not affect layout, so this does NOT touch the layout flag
         * nor propagate to ancestors — each widget owns its own draw commands.
         */
        void MarkRenderDirty();

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

        WeakRef<Widget> m_Parent;

        // This widget's own draw commands (excludes children). Regenerated by
        // BuildDrawCommands only when render-dirty; reused across frames otherwise.
        RenderBatch m_CachedCommands;

        // Layout dirty tracking. A widget starts dirty so the first frame always lays out.
        // m_LastArrangedSpace records the space received on the previous arrangement,
        // so a clean widget still re-arranges when its parent hands it a different rectangle.
        bool m_LayoutDirty = true;
        SRect m_LastArrangedSpace{};

        // Visual dirty flag (color/opacity/shadow/...).
        // Starts dirty so the first frame generates commands.
        bool m_RenderDirty = true;

        SRect m_Geometry{};
        glm::vec2 m_DesiredSize{};

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
        /**
         * Update the content widget: forwards the tick to the current content, if any.
         * @param frameTime time since the last tick.
         */
        void Update(Timestep frameTime) override;

        /**
         * Set the single child of this widget, replacing (and detaching) any previous
         * content. Adopts the new widget as a child and marks this widget's layout dirty.
         * @param widget the widget to host as content.
         * @return the content slot
         */
        ContentSlot& SetContent(const Ref<Widget>& widget);

        /**
         * Remove the current content: detaches the child (clearing its parent back-pointer)
         * and empties the content slot, marking this widget's layout dirty. No-op if there
         * is no content.
         */
        void ClearContent();

        /**
         * @return true if this widget currently hosts content, false otherwise.
         */
        bool HasContent() const { return m_ContentSlot != nullptr; }

        /**
         * Get the slot describing how the content is laid out (alignment, margin, ...).
         * @return the content slot, or nullptr if no content is set.
         */
        Ref<ContentSlot> GetContentSlot() const { return m_ContentSlot; }

      protected:
        /**
         * Remove the given widget if it is the current content: clears the content slot and
         * detaches the child (clearing its parent back-pointer), marking layout dirty. No-op
         * if the widget is not this widget's content.
         * @param child the widget to remove.
         */
        void RemoveChild(const Ref<Widget>& child) override;

        /**
         * Invoke fn with this widget's content, if any. Calls fn at most once, since a
         * ContentWidget hosts a single child; no-op when there is no content.
         * @param fn callback invoked with the content widget.
         */
        void ForEachChild(const std::function<void(const Ref<Widget>&)>& fn) const override;

        Ref<ContentSlot> m_ContentSlot;
    };
}