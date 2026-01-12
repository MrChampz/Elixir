#include "epch.h"
#include "Manager.h"

#include <Engine/GUI/Panel.h>
#include <Engine/Event/WindowEvent.h>

namespace Elixir::GUI
{
    void Manager::Initialize(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const Extent2D& extent
    )
    {
        m_Renderer = CreateScope<Renderer>();
        m_Renderer->Initialize(context, shaderLoader, extent);
        m_Initialized = true;
    }

    void Manager::Shutdown()
    {
        m_Renderer->Shutdown();
        m_Initialized = false;
    }

    void Manager::Tick(const Timestep frameTime) const
    {
        if (m_RootWidget)
            m_RootWidget->Tick(frameTime);
    }

    void Manager::Render()
    {
        if (!m_RootWidget || !m_RootWidget->IsVisible()) return;

        m_RenderBatch.Clear();
        m_RootWidget->GenerateDrawCommands(m_RenderBatch);
        m_RenderBatch.Sort();
        m_Renderer->Render(m_RenderBatch);
    }

    void Manager::OnWindowResize(const WindowResizeEvent& event) const
    {
        const Extent2D extent = { event.GetWidth(), event.GetHeight() };
        m_Renderer->Resize(extent);
        ArrangeLayout(extent);
    }

    void Manager::ArrangeLayout(const Extent2D& extent) const
    {
        if (m_RootWidget)
        {
            const SRect rootGeometry = { { 0, 0 }, { extent.Width, extent.Height } };
            m_RootWidget->SetGeometry(rootGeometry);

            if (const auto panel = std::dynamic_pointer_cast<Panel>(m_RootWidget))
            {
                panel->ArrangeChildren();
            }
        }
    }
}