#pragma once

#include <Engine/GUI/Renderer/Renderer.h>
#include <Engine/GUI/Panel.h>

namespace Elixir
{
    class WindowResizeEvent;
}

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

        void OnEvent(Event& event);
        bool OnWindowResize(const WindowResizeEvent& event) const;

        void SetRoot(const Ref<Panel>& root)
        {
            m_RootWidget = root;
        }

      private:
        void ProcessInput();
        void ProcessWidget(const Ref<Widget>& widget);
        void ProcessInputRecursive(const Ref<Widget>& widget);

        Scope<Renderer> m_Renderer;
        RenderBatch m_RenderBatch;

        Ref<Panel> m_RootWidget;
        Ref<Widget> m_PressedWidget;

        glm::vec2 m_MousePos{};
        bool m_WasMouseDown = false;
        bool m_MousePressed = false;
        bool m_MouseReleased = false;

        bool m_Initialized = false;
    };
}