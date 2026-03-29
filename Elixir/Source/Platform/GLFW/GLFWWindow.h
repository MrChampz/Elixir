#pragma once

#include <Engine/Core/Window.h>

#include <GLFW/glfw3.h>

namespace Elixir::GLFW
{
    struct WindowData
    {
        std::string Title;
        Extent2D WindowExtent;
        Extent2D FramebufferExtent;
        Window::EventCallbackFn EventCallback;
    };

    class GLFWWindow final : public Window
    {
      public:
        explicit GLFWWindow(const WindowProps& props);
        ~GLFWWindow() override;

        void Update() override;

        void SetTitle(const std::string& title) override;

        void SetEventCallback(const EventCallbackFn& callback) override
        {
            m_Data.EventCallback = callback;
        }

        void ShowFPSAndFrameTime(int fps, Timestep frameTime) override;

        const Extent2D& GetWindowExtent() const override { return m_Data.WindowExtent; }
        uint32_t GetWidth() const override { return m_Data.WindowExtent.Width; }
        uint32_t GetHeight() const override { return m_Data.WindowExtent.Height; }

        const Extent2D& GetFramebufferExtent() const override { return m_Data.FramebufferExtent; }

        float GetDPIScale() const override;

        void* GetHandle() const override { return m_Window; }

      private:
        void Init(const WindowProps& props);
        void Shutdown();

      private:
        GLFWwindow* m_Window;
        WindowData m_Data;
    };
}
