#pragma once

#include <Engine/GUI/Renderer/Renderer.h>
#include <Engine/GUI/InputManager.h>
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
            m_InputManager.SetRoot(m_RootWidget);
        }

      private:
        Scope<Renderer> m_Renderer;
        RenderBatch m_RenderBatch;
        InputManager m_InputManager;
        Ref<Panel> m_RootWidget;

        bool m_Initialized = false;
    };
}