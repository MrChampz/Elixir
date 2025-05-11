#pragma once

#include <Engine/Input/Input.h>

namespace Elixir
{
    class ELIXIR_API InputManager final
    {
    public:
        static void OnEvent(Event& event);

        static bool IsKeyPressed(int keyCode);

        static bool IsMouseButtonPressed(int button);
        static std::pair<float, float> GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();

        static bool IsGamepadConnected(int gamepadIndex);
        static bool IsGamepadButtonPressed(int gamepadIndex, int button);
        static float GetGamepadAxis(int gamepadIndex, int axis);
        static const char* GetGamepadName(int gamepadIndex);

        static Input& GetInput() { return *s_Input; }

    private:
        static Scope<Input> s_Input;
    };
}