#include "epch.h"
#include "Application.h"

#include <Engine/Input/InputManager.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Graphics/TextureLoader.h>

namespace Elixir
{
    Application* Application::s_Application = nullptr;

    Application::Application()
    {
        EE_PROFILE_ZONE_SCOPED()

        EE_CORE_ASSERT(!s_Application, "Application already exists!")
        s_Application = this;

        m_Window = Window::Create();
        m_Window->SetEventCallback(EE_BIND_EVENT_FN(Application::OnEvent));

        m_GraphicsContext = GraphicsContext::Create(EGraphicsAPI::Vulkan, m_Window.get());
        m_GraphicsContext->Init();

        TextureLoader::Initialize(m_GraphicsContext.get());
    }

    Application::~Application()
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext->WaitDeviceIdle();
    }

    void Application::Run()
    {
        EE_PROFILE_ZONE_SCOPED()

        while (m_Running)
        {
            EE_PROFILE_ZONE_SCOPED_NAMED("RunLoop")

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

            EE_PROFILE_FRAME_MARK()
        }

        m_GraphicsContext->WaitDeviceIdle();
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