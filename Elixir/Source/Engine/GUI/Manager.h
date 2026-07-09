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
        void ProcessWidget(const Ref<Widget>& widget);
        void ProcessInputRecursive(const Ref<Widget>& widget);

        static void ProcessKeyPressedRecursive(const Ref<Widget>& widget, const KeyPressedEvent& event);
        static void ProcessKeyTypedRecursive(const Ref<Widget>& widget, const KeyTypedEvent& event);

        Scope<Renderer> m_Renderer;
        RenderBatch m_RenderBatch;

        Ref<Panel> m_RootWidget;
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