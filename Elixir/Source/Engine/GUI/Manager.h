#pragma once

#include <Engine/Event/KeyEvent.h>
#include <Engine/Event/WindowEvent.h>
#include <Engine/GUI/Renderer/Renderer.h>
#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    /**
     * @brief One stacked layer of the UI.
     *
     * Index 0 in Manager::m_Layers is the always-present UI root (screen sized, filled by
     * SetRoot); anything above it is a popup (dropdown menu, tooltip, modal, ...) anchored
     * to a rect from  the layer below it and rendered, hit-tested and dismissed independently
     * of it.
     */
    struct SLayer
    {
        Ref<Widget> Root;
        SRect Anchor;
        bool DismissOnClickOutside = true;
    };

    class ELIXIR_API Manager
    {
    public:
        void Initialize(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const Extent2D& extent
        );
        void Shutdown();

        void ArrangeLayout(const Extent2D& extent) const;
        void Update(Timestep frameTime);
        void Render();

        void ProcessEvent(Event& event);

        void SetRoot(const Ref<Panel>& root);

        /**
         * @brief Push a new popup layer on top of the stack, anchored to a screen-space rect.
         *
         * (Typically the geometry of the widget that opened it, e.g. a menu bar button).
         *
         * Arranged immediately against the last extent ArrangeLayout ran with, so it has
         * correct geometry even before the next ArrangeLayout call.
         *
         * @param widget Root widget of the popup's own subtree.
         * @param anchor Screen-space rect the popup is positioned relative to.
         */
        void PushPopup(const Ref<Widget>& widget, const SRect& anchor);

        /**
         * @brief Pop the topmost popup layer.
         *
         * No-op when there are no popups - layer 0, the UI root, is never popped this way.
         */
        void PopPopup();

        /**
         * @brief Pop every popup layers, leaving only the UI root.
         */
        void ClearPopups();

        /**
         * @brief Get the number of stacked popup layers above the root layer.
         * @return Number of popup layers currently stacked above the root layer (0 if none).
         */
        size_t GetPopupCount() const { return m_Layers.empty() ? 0 : m_Layers.size() - 1; }

        /**
         * @brief True if the GUI currently wants mouse input: the hover path is non-empty or
         * a widget is capturing the mouse.
         *
         * Lets a consumer (e.g. an editor camera controller polling its own mouse input) skips
         * its own handling while the user is interacting with the GUI instead.
         *
         * @return True if the GUI currently wants mouse input.
         */
        bool WantsMouse() const;

        const RenderBatch& GetRenderBatch() const { return m_RenderBatch; }

    protected:
        void AssembleFrame();

        bool NeedsRebuild() const;
        void MarkRebuilt();

        // Not const: Tab/Shift+Tab/Escape are intercepted here, before the bubble to
        // m_FocusedWidget, and moving/clearing focus mutates m_FocusedWidget and the cached
        // focus order. See the ordering rationale on the .cpp definition. Protected (not
        // private) so tests can drive it directly - see ManagerTestUtils.h.
        bool HandleKeyPressed(const KeyPressedEvent& event);

        // Bubbles a mouse-down left -> root over path until a widget handles it; that widget
        // becomes m_PressedWidget (and, if it asked, m_MouseCapture) and gains focus. If
        // nobody handles it, treats the press as "clicked outside and clears focus. Protected
        // (not private) so tests can drive it directly - see ManagerTestUtils.h.
        void ProcessMousePress(const std::vector<Ref<Widget>>& path);

        // Common focus-change plumbing: fires HandleLostFocus/HandleFocus only when the
        // focused widget actually changes; widget may be nullptr to clear focus. Protected
        // (not private) so tests can drive it directly - see ManagerTestUtils.h.
        void SetFocusedWidget(const Ref<Widget>& widget);

    private:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) const;
        bool HandleKeyTyped(const KeyTypedEvent& event) const;

        // Bubbles a wheel tick leaf -> root over m_HoverPath, stopping at the first
        // widget whose HandleMouseScrolled reports EventHandled.
        bool HandleMouseScrolled(const MouseScrolledEvent& event) const;

        void ProcessInput();

        // Diffs the freshly hit-tested path against m_HoverPath, firing HandleMouseLeave
        // (leaf -> root) on widgets that fell out and HandleMouseEnter (root -> leaf) on
        // widgets that newly entered, then stores path as the new m_HoverPath.
        void UpdateHoverPath(const std::vector<Ref<Widget>>& path);

        // Routes mouse-up to m_MouseCapture if set, otherwise bubbles over path; then
        // synthesizes HandleClick on m_PressedWidget if it is still present in path.
        void ProcessMouseRelease(const std::vector<Ref<Widget>>& path);

        // Routes mouse-move to m_MouseCapture if set, otherwise bubbles over path.
        void ProcessMouseMove(const std::vector<Ref<Widget>>& path);

        // Depth-first, first-child-first walk collecting every focusable
        // (Widget::IsFocusable) and keyboard-reachable widget under widget, in traversal
        // order. Prunes the same HitTestInvisible/Hidden/Collapsed branches Widget::HitTest
        // prunes, and likewise skips (without excluding descendants of) a
        // SelfHitTestInvisible widget - same visibility contract, reused rather than reinvented,
        // just walked root -> leaf instead of HitTest's leaf-seeking back-to-front order,
        // since Tab order is reading order, not z-order.
        static void CollectFocusOrder(const Ref<Widget>& widget, std::vector<Ref<Widget>>& out);

        // Lazily rebuilds the cached focus order.
        const std::vector<Ref<Widget>>& GetFocusOrder();

        // Move focus to the next entry in GetFocusOrder().
        void FocusNext();

        // Move focus to the previous entry in GetFocusOrder().
        void FocusPrevious();

        // Topmost layer whose geometry contains point; falls back to layer 0 (the UI root
        // always "hits" - its geometry covers the whole screen).
        const SLayer& GetTopmostHitLayer(const glm::vec2& point) const;

        // Pops layers from the top while DismissOnClickOutside is set and the layer's
        // geometry does not contain point. Stops at the first layer that either contains
        // the point or opted out of dismiss-on-click-outside.
        void DismissPopupsOutside(const glm::vec2& point);

        // anchor + a popup's own desired size -> a rect that fits on screen: opens below
        // the anchor by default, flips above when it wouldn't fit below, and is finally
        // clamped fully inside screenRect as a last resort.
        static SRect ComputePopupRect(
            const SRect& anchor,
            const glm::vec2& desiredSize,
            const SRect& screenRect
        );

        Scope<Renderer> m_Renderer;
        RenderBatch m_RenderBatch;

        // Index 0 is the UI root; anything above it is a popup, topmost last.
        std::vector<SLayer> m_Layers;

        // Widgets currently under the cursor, root -> leaf. Diffed every frame in
        // UpdateHoverPath to drive HandleMouseEnter/HandleMouseLeave.
        std::vector<Ref<Widget>> m_HoverPath;

        // Widget that captured the mouse on press, if any.
        // While set, mouse move/up go straight to it regardless of the hover path.
        WeakRef<Widget> m_MouseCapture;

        // Widget that consumed the last mouse-down (the "down" target), kept until the
        // matching release purely to synthesize HandleClick when the release still lands
        // on it - it is NOT a second, hand-rolled capture path.
        Ref<Widget> m_PressedWidget;

        Ref<Widget> m_FocusedWidget;

        // Cache behind GetFocusOrder: the widgets currently eligible for focus,
        // in traversal order, scoped to the topmost layer at the time of the last rebuild.
        // Keyed the same way NeedsRebuild keys the render batch - epoch + layer stack
        // version - and rebuilt lazily on the next FocusNext/FocusPrevious call, not eagerly
        // on every mutation.
        std::vector<Ref<Widget>> m_FocusOrder;
        uint64_t m_FocusOrderEpoch = 0;
        uint64_t m_FocusOrderLayerVersion = 0;

        glm::vec2 m_MousePos{};
        glm::vec2 m_LastMousePos{};
        bool m_WasMouseDown = false;
        bool m_MousePressed = false;
        bool m_MouseReleased = false;
        bool m_MouseMoved = false;

        // Last extent passed to ArrangeLayout, so a popup pushed mid-frame (after this
        // frame's ArrangeLayout already ran) can still be arranged immediately instead of
        // rendering at a stale {0,0} geometry for one frame.
        mutable Extent2D m_LastExtent{};

        // Dirty epoch of the last frame we assembled + uploaded. When it still matches the
        // current epoch, the batch and GPU buffers are reused and only the draws are re-issued.
        uint64_t m_LastRenderedEpoch = 0;

        // Bumped on every layer stack mutation (SetRoot, PushPopup, PopPopup, ClearPopups).
        // A layer change doesn't necessarily bump Widget::CurrentDirtyEpoch - a freshly built
        // popup subtree starts dirty by construction, without ever calling MarkLayoutDirty -
        // so the epoch comparison alone can't detect "a popup was opened"; this can.
        uint64_t m_LayerStackVersion = 0;
        uint64_t m_LastRenderedLayerVersion = 0;

        bool m_Initialized = false;
    };
}
