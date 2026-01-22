#pragma once

#include <Engine/GUI/Renderer.h>
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
        void Tick(Timestep frameTime) const;
        void Render();

        void OnWindowResize(const WindowResizeEvent& event) const;

        void SetRoot(const Ref<Panel>& root) { m_RootWidget = root; }

      private:
        Scope<Renderer> m_Renderer;
        RenderBatch m_RenderBatch;
        Ref<Panel> m_RootWidget;

        bool m_Initialized = false;
    };
}