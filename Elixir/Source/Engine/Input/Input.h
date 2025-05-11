#pragma once

#include <Engine/Event/MouseEvent.h>

namespace Elixir
{
    class ELIXIR_API Input
    {
    public:
        virtual ~Input() = default;

        virtual void OnMouseMoved(MouseMovedEvent& event)
        {
            m_MousePosition = event.GetPosition();
        }

        virtual bool IsKeyPressed(int keyCode) = 0;
        virtual bool IsMouseButtonPressed(int button) = 0;

        virtual std::pair<float, float> GetMousePosition() { return m_MousePosition; }
        virtual float GetMouseX() { return m_MousePosition.first; }
        virtual float GetMouseY() { return m_MousePosition.second; }

        virtual bool IsGamepadConnected(int gamepadIndex) = 0;
        virtual bool IsGamepadButtonPressed(int gamepadIndex, int button) = 0;
        virtual float GetGamepadAxis(int gamepadIndex, int axis) = 0;
        virtual const char* GetGamepadName(int gamepadIndex) = 0;

    protected:
        std::pair<float, float> m_MousePosition;
    };
}