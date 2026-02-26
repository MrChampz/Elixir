#include "epch.h"
#include "Manager.h"

#include <Engine/GUI/Panel.h>
#include <Engine/Event/WindowEvent.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>

namespace Elixir::GUI
{
    void Manager::Initialize(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const Extent2D& extent
    )
    {
        m_Renderer = CreateScope<Renderer>(context, shaderLoader, extent);
        m_Initialized = true;
    }

    void Manager::Shutdown()
    {
        m_Initialized = false;
    }

    void Manager::ArrangeLayout(const Extent2D& extent) const
    {
        if (m_RootWidget)
        {
            const SRect rootGeometry = { { 0, 0 }, { extent.Width, extent.Height } };
            m_RootWidget->ArrangeChildren(rootGeometry);
        }
    }

    void Manager::Update(const Timestep frameTime)
    {
        ProcessInput();

        if (m_RootWidget)
            m_RootWidget->Update(frameTime);
    }

    void Manager::Render()
    {
        if (!m_RootWidget || !m_RootWidget->IsVisible()) return;

        m_RenderBatch.Clear();
        m_RootWidget->GenerateDrawCommands(m_RenderBatch);
        m_RenderBatch.Sort();
        m_Renderer->Render(m_RenderBatch);
    }

    void Manager::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>(EE_BIND_EVENT_FN(Manager::OnWindowResize));
    }

    bool Manager::OnWindowResize(const WindowResizeEvent& event) const
    {
        const Extent2D extent = { event.GetWidth(), event.GetHeight() };
        m_Renderer->Resize(extent);
        ArrangeLayout(extent);

        return false;
    }

    void Manager::ProcessInput()
    {
        const auto [x, y] = InputManager::GetMousePosition();
        m_MousePos = { x, y };

        const auto isMouseDown = InputManager::IsMouseButtonDown(EE_MOUSE_BUTTON_LEFT);

        m_MousePressed = isMouseDown && !m_WasMouseDown;
        m_MouseReleased = !isMouseDown && m_WasMouseDown;

        m_WasMouseDown = isMouseDown;

        if (m_RootWidget)
        {
            ProcessInputRecursive(m_RootWidget);

            if (m_MouseReleased && m_PressedWidget)
            {
                m_PressedWidget->HandleMouseUp();
                m_PressedWidget = nullptr;
            }
        }
    }

    void Manager::ProcessWidget(const Ref<Widget>& widget)
    {
        if (!widget || !widget->IsVisible()) return;

        const auto geometry = widget->GetGeometry();
        const bool isOver = geometry.Contains(m_MousePos);

        if (isOver && !widget->IsHovered())
        {
            widget->HandleMouseEnter();
        }
        else if (!isOver && widget->IsHovered())
        {
            widget->HandleMouseLeave();
        }

        if (isOver && m_MousePressed)
        {
            widget->HandleMouseDown();
            m_PressedWidget = widget;
        }

        if (isOver && m_MouseReleased)
        {
            widget->HandleMouseUp();

            if (widget->IsPressed() && m_PressedWidget == widget)
                widget->HandleClick();
        }
    }

    void Manager::ProcessInputRecursive(const Ref<Widget>& widget)
    {
        ProcessWidget(widget);

        if (const auto contentWidget = std::dynamic_pointer_cast<ContentWidget>(widget))
        {
            if (const auto slot = contentWidget->GetContentSlot())
            {
                ProcessInputRecursive(slot->GetWidget());
            }
        }
        else if (const auto panel = std::dynamic_pointer_cast<Panel>(widget))
        {
            for (auto& slot : panel->GetSlots())
            {
                ProcessInputRecursive(slot->GetWidget());
            }
        }
    }
}