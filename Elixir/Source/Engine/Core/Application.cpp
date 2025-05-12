#include "epch.h"
#include "Application.h"

#include <Engine/Input/InputManager.h>
#include <Engine/Input/InputCodes.h>

#ifdef EE_DEBUG
#include <tracy/Tracy.hpp>
#endif

namespace Elixir
{
    Application* Application::s_Application = nullptr;

    Application::Application()
    {
        EE_CORE_ASSERT(!s_Application, "Application already exists!")
        s_Application = this;

        m_Window = Window::Create();
        m_Window->SetEventCallback(EE_BIND_EVENT_FN(Application::OnEvent));
    }

    Application::~Application()
    {

    }

    void Application::Run()
    {
        while (m_Running)
        {
            const auto frameTime = m_Timer.GetLastFrameTime();

            if (!m_Minimized)
            {
                m_Window->ShowFPSAndFrameTime(m_Profiler.GetFPS(), frameTime);

                OnGUI(frameTime);

                m_Profiler.OnUpdate(frameTime);
            }

            m_Window->OnUpdate();

            if (InputManager::IsKeyPressed(EE_KEY_ESCAPE))
            {
                auto event = WindowCloseEvent();
                OnWindowClose(event);
            }

#ifdef EE_DEBUG
            FrameMark;
#endif
        }
    }

    void Application::OnGUI(Timestep frameTime) {}

    void Application::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(EE_BIND_EVENT_FN(Application::OnWindowClose));
        dispatcher.Dispatch<WindowResizeEvent>(EE_BIND_EVENT_FN(Application::OnWindowResize));

        InputManager::OnEvent(event);
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& event)
    {
        EE_CORE_INFO("{0}", event)

        if (event.GetWidth() == 0 || event.GetHeight() == 0)
        {
            m_Minimized = true;
            return false;
        }

        m_Minimized = false;

        return false;
    }
}