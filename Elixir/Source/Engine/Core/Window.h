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
        unsigned int Width;
        unsigned int Height;

        explicit WindowProps(
            std::string title = "Project Elixir",
            unsigned int width = 1280,
            unsigned int height = 720
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

        virtual void OnUpdate() = 0;

        virtual void SetTitle(const std::string& title) = 0;
        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void ShowFPSAndFrameTime(int fps, Timestep frameTime) = 0;

        [[nodiscard]] virtual unsigned int GetWidth() const = 0;
        [[nodiscard]] virtual unsigned int GetHeight() const = 0;
        [[nodiscard]] virtual void* GetNativeWindow() const = 0;

        static Scope<Window> Create(const WindowProps& props = WindowProps());
    };
}
