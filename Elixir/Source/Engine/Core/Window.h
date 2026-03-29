#pragma once
#include "epch.h"

#include <Engine/Core/Core.h>
#include <Engine/Core/Timer.h>
#include <Engine/Event/Event.h>

namespace Elixir
{
    struct WindowProps
    {
        std::string Title;
        uint32_t Width;
        uint32_t Height;

        explicit WindowProps(
            std::string title = "Project Elixir",
            uint32_t width = 1280,
            uint32_t height = 720
        ) : Title(std::move(title)), Width(width), Height(height) {}
    };

    /**
     * @brief Interface representing a desktop system based Window.
     */
    class ELIXIR_API Window
    {
      public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual void Update() = 0;

        virtual void SetTitle(const std::string& title) = 0;
        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void ShowFPSAndFrameTime(int fps, Timestep frameTime) = 0;

        virtual const Extent2D& GetWindowExtent() const = 0;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual const Extent2D& GetFramebufferExtent() const = 0;

        virtual float GetDPIScale() const = 0;

        virtual void* GetHandle() const = 0;

        static Scope<Window> Create(const WindowProps& props = WindowProps());
    };
}
