#include "epch.h"
#include "InputManager.h"

#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>

namespace Elixir::GUI
{
    void InputManager::Update()
    {
        const auto [x, y] = ::InputManager::GetMousePosition();
        m_MousePos = { x, y };

        const auto isMouseDown = ::InputManager::IsMouseButtonDown(EE_MOUSE_BUTTON_LEFT);

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

    void InputManager::ProcessWidget(const Ref<Widget>& widget)
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

            if (m_PressedWidget == widget)
                widget->HandleClick();
        }
    }

    void InputManager::ProcessInputRecursive(const Ref<Widget>& widget)
    {
        ProcessWidget(widget);

        if (const auto panel = std::dynamic_pointer_cast<Panel>(widget))
        {
            for (auto& slot : panel->GetSlots())
            {
                ProcessInputRecursive(slot->GetWidget());
            }
        }
    }
}