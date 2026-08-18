#pragma once

#include <Engine/Event/KeyEvent.h>
#include <Engine/Event/WindowEvent.h>
#include <Engine/GUI/Renderer/Renderer.h>
#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
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

        void SetRoot(const Ref<Panel>& root)
        {
            m_RootWidget = root;
        }

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

    private:
        bool HandleFramebufferResize(const FramebufferResizeEvent& event) const;
        bool HandleKeyPressed(const KeyPressedEvent& event) const;
        bool HandleKeyTyped(const KeyTypedEvent& event) const;

        void ProcessInput();

        // Diffs the freshly hit-tested path against m_HoverPath, firing HandleMouseLeave
        // (leaf -> root) on widgets that fell out and HandleMouseEnter (root -> leaf) on
        // widgets that newly entered, then stores path as the new m_HoverPath.
        void UpdateHoverPath(const std::vector<Ref<Widget>>& path);

        // Bubbles a mouse-down left -> root over path until a widget handles it; that widget
        // becomes m_PressedWidget (and, if it asked, m_MouseCapture) and gains focus. If
        // nobody handles it, treats the press as "clicked outside and clears focus.
        void ProcessMousePress(const std::vector<Ref<Widget>>& path);

        // Routes mouse-up to m_MouseCapture if set, otherwise bubbles over path; then
        // synthesizes HandleClick on m_PressedWidget if it is still present in path.
        void ProcessMouseRelease(const std::vector<Ref<Widget>>& path);

        // Routes mouse-move to m_MouseCapture if set, otherwise bubbles over path.
        void ProcessMouseMove(const std::vector<Ref<Widget>>& path);

        // Common focus-change plumbing: fires HandleLostFocus/HandleFocus only when the
        // focused widget actually changes; widget may be nullptr to clear focus.
        void SetFocusedWidget(const Ref<Widget>& widget);

        Scope<Renderer> m_Renderer;
        RenderBatch m_RenderBatch;

        Ref<Panel> m_RootWidget;

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

        glm::vec2 m_MousePos{};
        glm::vec2 m_LastMousePos{};
        bool m_WasMouseDown = false;
        bool m_MousePressed = false;
        bool m_MouseReleased = false;
        bool m_MouseMoved = false;

        // Dirty epoch of the last frame we assembled + uploaded. When it still matches the
        // current epoch, the batch and GPU buffers are reused and only the draws are re-issued.
        uint64_t m_LastRenderedEpoch = 0;

        // Tracks the last rendered panel, so when changed, can rebuild the render batch.
        WeakRef<Panel> m_LastRenderedRoot;

        bool m_Initialized = false;
    };
}