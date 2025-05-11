#include "epch.h"
#include "InputManager.h"

namespace Elixir
{
    void AssertInputInitialized(const Input* input)
    {
        EE_CORE_ASSERT(input, "Input system not initialized!")
    }

    void InputManager::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseMovedEvent>([&](MouseMovedEvent& ev)
        {
            s_Input->OnMouseMoved(ev);
            return true;
        });
    }

    bool InputManager::IsKeyPressed(const int keyCode)
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->IsKeyPressed(keyCode);
    }

    bool InputManager::IsMouseButtonPressed(const int button)
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->IsMouseButtonPressed(button);
    }

    std::pair<float, float> InputManager::GetMousePosition()
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->GetMousePosition();
    }

    float InputManager::GetMouseX()
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->GetMouseX();
    }

    float InputManager::GetMouseY()
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->GetMouseY();
    }

    bool InputManager::IsGamepadConnected(const int gamepadIndex)
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->IsGamepadConnected(gamepadIndex);
    }

    bool InputManager::IsGamepadButtonPressed(const int gamepadIndex, const int button)
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->IsGamepadButtonPressed(gamepadIndex, button);
    }

    float InputManager::GetGamepadAxis(const int gamepadIndex, const int axis)
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->GetGamepadAxis(gamepadIndex, axis);
    }

    const char* InputManager::GetGamepadName(const int gamepadIndex)
    {
        AssertInputInitialized(s_Input.get());
        return s_Input->GetGamepadName(gamepadIndex);
    }
}