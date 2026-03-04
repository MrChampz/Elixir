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
        dispatcher.Dispatch<WindowResizeEvent>(EE_BIND_EVENT_FN(Manager::HandleWindowResize));
        dispatcher.Dispatch<KeyPressedEvent>(EE_BIND_EVENT_FN(Manager::HandleKeyPressed));
        dispatcher.Dispatch<KeyTypedEvent>(EE_BIND_EVENT_FN(Manager::HandleKeyTyped));
    }

    bool Manager::HandleWindowResize(const WindowResizeEvent& event) const
    {
        const Extent2D extent = { event.GetWidth(), event.GetHeight() };
        m_Renderer->Resize(extent);
        ArrangeLayout(extent);

        return true;
    }

    bool Manager::HandleKeyPressed(const KeyPressedEvent& event)
    {
        if (m_FocusedWidget)
        {
            ProcessKeyPressedRecursive(m_FocusedWidget, event);
        }

        return true;
    }

    bool Manager::HandleKeyTyped(const KeyTypedEvent& event)
    {
        if (m_FocusedWidget)
        {
            ProcessKeyTypedRecursive(m_FocusedWidget, event);
        }

        return true;
    }

    void Manager::ProcessInput()
    {
        const auto [x, y] = InputManager::GetMousePosition();
        m_LastMousePos = m_MousePos;
        m_MousePos = { x, y };
        m_MouseMoved = m_MousePos != m_LastMousePos;

        const auto isMouseDown = InputManager::IsMouseButtonDown(EE_MOUSE_BUTTON_LEFT);

        m_MousePressed = isMouseDown && !m_WasMouseDown;
        m_MouseReleased = !isMouseDown && m_WasMouseDown;
        m_WasMouseDown = isMouseDown;

        if (m_RootWidget)
        {
            ProcessInputRecursive(m_RootWidget);

            if (m_MouseReleased && m_PressedWidget)
            {
                const auto event = MouseButtonReleasedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);
                m_PressedWidget->HandleMouseUp(event);
                m_PressedWidget = nullptr;
            }

            // If user clicked but nothing captured focus, clear it
            if (m_MousePressed && !m_PressedWidget && m_FocusedWidget)
            {
                m_FocusedWidget->HandleLostFocus();
                m_FocusedWidget = nullptr;
            }

            // Mouse move, notify pressed widget (for dragging/selection)
            if (m_MouseMoved && m_PressedWidget)
            {
                const auto event = MouseMovedEvent(m_MousePos);
                m_PressedWidget->HandleMouseMove(event);
            }
        }
    }

    void Manager::ProcessWidget(const Ref<Widget>& widget)
    {
        if (!widget || !widget->IsVisible()) return;

        const auto geometry = widget->GetGeometry();
        const bool isOver = geometry.Contains(m_MousePos);

        // Hover
        if (isOver && !widget->IsHovered())
        {
            widget->HandleMouseEnter();
        }
        else if (!isOver && widget->IsHovered())
        {
            widget->HandleMouseLeave();
        }

        // Press + Focus
        if (isOver && m_MousePressed)
        {
            const auto event = MouseButtonPressedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);
            widget->HandleMouseDown(event);
            m_PressedWidget = widget;

            // Focus: only change if clicking a different widget
            if (m_FocusedWidget != widget)
            {
                if (m_FocusedWidget)
                    m_FocusedWidget->HandleLostFocus();

                m_FocusedWidget = widget;
                m_FocusedWidget->HandleFocus();
            }
        }

        // Click
        if (isOver && m_MouseReleased)
        {
            const auto event = MouseButtonReleasedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);
            widget->HandleMouseUp(event);

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

    void Manager::ProcessKeyPressedRecursive(
        const Ref<Widget>& widget,
        const KeyPressedEvent& event
    )
    {
        widget->HandleKeyPressed(event);

        if (const auto contentWidget = std::dynamic_pointer_cast<ContentWidget>(widget))
        {
            if (const auto slot = contentWidget->GetContentSlot())
            {
                ProcessKeyPressedRecursive(slot->GetWidget(), event);
            }
        }
        else if (const auto panel = std::dynamic_pointer_cast<Panel>(widget))
        {
            for (auto& slot : panel->GetSlots())
            {
                ProcessKeyPressedRecursive(slot->GetWidget(), event);
            }
        }
    }

    void Manager::ProcessKeyTypedRecursive(const Ref<Widget>& widget, const KeyTypedEvent& event)
    {
        widget->HandleKeyTyped(event);

        if (const auto contentWidget = std::dynamic_pointer_cast<ContentWidget>(widget))
        {
            if (const auto slot = contentWidget->GetContentSlot())
            {
                ProcessKeyTypedRecursive(slot->GetWidget(), event);
            }
        }
        else if (const auto panel = std::dynamic_pointer_cast<Panel>(widget))
        {
            for (auto& slot : panel->GetSlots())
            {
                ProcessKeyTypedRecursive(slot->GetWidget(), event);
            }
        }
    }
}