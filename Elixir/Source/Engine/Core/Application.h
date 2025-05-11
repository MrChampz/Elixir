#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/Window.h>
#include <Engine/Core/Timer.h>
#include <Engine/Core/FrameProfiler.h>
#include <Engine/Event/Event.h>
#include <Engine/Event/WindowEvent.h>

namespace Elixir
{
    class ELIXIR_API Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();

        virtual void OnGUI(Timestep frameTime);
        void OnEvent(Event& event);

        [[nodiscard]] const Window* GetWindow() const { return m_Window.get(); }

        static Application& Get() { return *s_Application; }

    protected:
        bool OnWindowClose(WindowCloseEvent& event);
        bool OnWindowResize(WindowResizeEvent& event);

        Scope<Window> m_Window;

        Timer m_Timer;
        FrameProfiler m_Profiler;
        bool m_Running = true;
        bool m_Minimized = false;

    private:
        static Application* s_Application;
    };

    // NOTE: To be defined in client
    extern Application* CreateApplication();
}