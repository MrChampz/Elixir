#include "epch.h"
#include "Application.h"

#include "Engine/GUI/FontManager.h"
#include "Engine/GUI/Button.h"
#include "Engine/GUI/Canvas.h"
#include "Engine/GUI/HorizontalBox.h"
#include "Engine/GUI/Overlay.h"

#include <Engine/GUI/VerticalBox.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Graphics/TextureLoader.h>

namespace Elixir
{
    Application* Application::s_Application = nullptr;

    Application::Application() : m_Executor(Executor::Get())
    {
        EE_PROFILE_ZONE_SCOPED()

        EE_CORE_ASSERT(!s_Application, "Application already exists!")
        s_Application = this;

        m_Window = Window::Create();
        m_Window->SetEventCallback(EE_BIND_EVENT_FN(Application::OnEvent));

        m_GraphicsContext = GraphicsContext::Create(EGraphicsAPI::Vulkan, &m_Executor, m_Window.get());
        m_GraphicsContext->Init();

        m_ShaderLoader = CreateScope<ShaderLoader>(m_GraphicsContext.get());

        TextureLoader::Initialize(m_GraphicsContext.get());
        GUI::FontManager::Initialize(m_GraphicsContext.get());

        m_GUIManager = CreateScope<GUI::Manager>();
        m_GUIManager->Initialize(
            m_GraphicsContext.get(),
            m_ShaderLoader.get(),
            { m_Window->GetWidth(), m_Window->GetHeight() }
        );

        const auto panel = CreateRef<GUI::Canvas>();
        panel->SetBackground({ 1.0f, 0.0f, 0.0f, 1.0f });
        panel->SetPadding({ 10, 20, 10, 10 });
        const auto button = CreateRef<GUI::Button>("Hello World");
        button->SetCornerRadius(4.0);

        const auto button2 = CreateRef<GUI::Button>();
        button2->SetNormalColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        button2->SetCornerRadius(12);

        panel->AddChild(button)
            .SetAnchors(GUI::SAnchors::TopLeft())
            .SetPosition({ 10, 10 })
            .SetSize({ 100, 40 });

        auto x = panel->AddChild(button2)
            .SetAnchors(GUI::SAnchors::MiddleCenter())
            .SetAlignment({ 0.5f, 0.5f });

        m_GUIManager->SetRoot(panel);
    }

    Application::~Application()
    {
        EE_PROFILE_ZONE_SCOPED()
        GUI::FontManager::Shutdown();
        m_Running = false;
    }

    const auto EE_PROFILE_MAIN_LOOP = "Main Loop";

    void Application::Run()
    {
        EE_PROFILE_ZONE_SCOPED()

        while (m_Running)
        {
            EE_PROFILE_ZONE_SCOPED_NAMED("RunLoop")

            const auto frameTime = m_Timer.GetLastFrameTime();

            m_Profiler.OnUpdate(frameTime);
            m_Window->OnUpdate();

            if (InputManager::IsKeyPressed(EE_KEY_ESCAPE))
            {
                auto event = WindowCloseEvent();
                OnWindowClose(event);
            }

            if (m_Minimized)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            m_Window->ShowFPSAndFrameTime(m_Profiler.GetFPS(), frameTime);

            OnGUI(frameTime);
            m_GUIManager->ArrangeLayout({ m_Window->GetWidth(), m_Window->GetHeight() });
            m_GUIManager->Tick(frameTime);

            m_GraphicsContext->RenderFrame([this, frameTime]()
            {
                OnRender(frameTime);
                m_GUIManager->Render();
            });

            EE_PROFILE_FRAME_MARK_NAMED(EE_PROFILE_MAIN_LOOP)
        }

        m_GraphicsContext->DrainRenderQueue();
        EE_CORE_INFO("Finished running app!")
    }

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

        m_GUIManager->OnWindowResize(event);

        return false;
    }
}
