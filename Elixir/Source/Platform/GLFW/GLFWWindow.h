#pragma once

#include <Engine/Core/Window.h>

#include <GLFW/glfw3.h>

namespace Elixir::Platform::GLFW
{
    struct WindowData
    {
        std::string Title;
        unsigned int Width, Height;
        Window::EventCallbackFn EventCallback;
    };

    class GLFWWindow : public Window
    {
      public:
        explicit GLFWWindow(const WindowProps& props);
        ~GLFWWindow() override;

        void OnUpdate() override;

        void SetTitle(const std::string& title) override;

        void SetEventCallback(const EventCallbackFn& callback) override
        {
            m_Data.EventCallback = callback;
        }

        void ShowFPSAndFrameTime(int fps, Timestep frameTime) override;

        [[nodiscard]] unsigned int GetWidth() const override { return m_Data.Width; }
        [[nodiscard]] unsigned int GetHeight() const override { return m_Data.Height; }

        [[nodiscard]] void* GetNativeWindow() const override { return m_Window; }

      private:
        void Init(const WindowProps& props);
        void Shutdown();

      private:
        GLFWwindow* m_Window;
        WindowData m_Data;
    };
}
