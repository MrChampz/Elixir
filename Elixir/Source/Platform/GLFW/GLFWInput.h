#pragma once

#include <Engine/Input/Input.h>

namespace Elixir::Platform::GLFW
{
    class GLFWInput final : public Input
    {
    public:
        bool IsKeyPressed(int keyCode) override;
        bool IsMouseButtonPressed(int button) override;

        bool IsGamepadConnected(int gamepadIndex) override;
        bool IsGamepadButtonPressed(int gamepadIndex, int button) override;
        float GetGamepadAxis(int gamepadIndex, int axis) override;
        const char* GetGamepadName(int gamepadIndex) override;
    };
}
