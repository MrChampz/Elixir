#include "epch.h"
#include "GLFWInput.h"

#include <Engine/Core/Application.h>
#include <Engine/Input/InputManager.h>

#include <GLFW/glfw3.h>

namespace Elixir
{
    Scope<Input> InputManager::s_Input = CreateScope<Platform::GLFW::GLFWInput>();
}

namespace Elixir::Platform::GLFW
{
    GLFWwindow* GetWindow()
    {
        const auto window = Application::Get().GetWindow()->GetNativeWindow();
        return static_cast<GLFWwindow*>(window);
    }

    bool GLFWInput::IsKeyPressed(const int keyCode)
    {
        const auto window = GetWindow();
        const auto state = glfwGetKey(window, keyCode);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool GLFWInput::IsMouseButtonPressed(const int button)
    {
        const auto window = GetWindow();
        const auto state = glfwGetMouseButton(window, button);
        return state == GLFW_PRESS;
    }

    bool GLFWInput::IsGamepadConnected(const int gamepadIndex)
    {
        return glfwJoystickPresent(gamepadIndex);
    }

    bool GLFWInput::IsGamepadButtonPressed(const int gamepadIndex, const int button)
    {
        int count;
        const unsigned char* buttons = glfwGetJoystickButtons(gamepadIndex, &count);
        return (button < count) ? buttons[button] == GLFW_PRESS : false;
    }

    float GLFWInput::GetGamepadAxis(const int gamepadIndex, const int axis)
    {
        int count;
        const float* axes = glfwGetJoystickAxes(gamepadIndex, &count);
        return (axis < count) ? axes[axis] : 0.0f;
    }

    const char* GLFWInput::GetGamepadName(int gamepadIndex)
    {
        return glfwGetJoystickName(gamepadIndex);
    }
}