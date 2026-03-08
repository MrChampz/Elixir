#include "epch.h"
#include "Application.h"

#include "Engine/GUI/Button.h"
#include "Engine/GUI/Canvas.h"
#include "Engine/GUI/HorizontalBox.h"
#include "Engine/GUI/Overlay.h"
#include "Engine/GUI/TextBlock.h"
#include "Engine/GUI/TextField.h"

#include <Engine/Core/Platform.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Font/FontManager.h>
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

        Platform::Initialize(m_Window.get());

        m_GraphicsContext = GraphicsContext::Create(EGraphicsAPI::Vulkan, &m_Executor, m_Window.get());
        m_GraphicsContext->Init();

        m_ShaderLoader = CreateScope<ShaderLoader>(m_GraphicsContext.get());

        TextureLoader::Initialize(m_GraphicsContext.get());
        FontManager::Initialize(m_GraphicsContext.get());

        m_GUIManager = CreateScope<GUI::Manager>();
        m_GUIManager->Initialize(
            m_GraphicsContext.get(),
            m_ShaderLoader.get(),
            { m_Window->GetWidth(), m_Window->GetHeight() }
        );

        const auto buttonBg = TextureLoader::Load("./Assets/Button_Background.png");

        const auto panel = CreateRef<GUI::Canvas>();
        //panel->SetBackground({ 1.0f, 0.0f, 0.0f, 1.0f });
        panel->SetPadding({ 10, 20, 10, 10 });
        const auto button = CreateRef<GUI::Button>("Hello World until 2020");
        button->SetCornerRadius(4.0);
        button->SetNormalBackground(std::dynamic_pointer_cast<Texture2D>(buttonBg));
        button->SetPadding({ 20.0f, 0.0f });

        const auto button2 = CreateRef<GUI::Button>();
        button2->SetNormalColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        button2->SetHoverColor({ 0.8f, 0.8f, 1.0f, 1.0f });
        //button2->SetCornerRadius(12);
        button2->SetInsetShadow({ 10, 10    , 2, 0.3 });
        button2->SetDropShadow({ 20, 20, 10, 1 });
        button2->SetOutline({ { 1, 1, 0, 1 }, 5.0f });
        button2->OnMouseEnter([&]() { EE_CORE_INFO("Mouse entered button!"); });
        button2->OnMouseLeave([&]() { EE_CORE_INFO("Mouse left button!"); });
        button2->OnMouseDown([&]() { EE_CORE_INFO("Mouse down on button!"); });
        button2->OnMouseUp([&]() { EE_CORE_INFO("Mouse up on button!"); });
        button2->OnClick([&]() { EE_CORE_INFO("Button clicked!"); });

        const auto txt2 = CreateRef<GUI::TextBlock>("Button Button");
        txt2->SetColor({ 0.0, 1.0f, 0.0f, 1.0f });
        button2->SetContent(txt2)
            .SetHorizontalAlignment(EHorizontalAlignment::Right)
            .SetVerticalAlignment(EVerticalAlignment::Center)
            .SetMargin({ 10, 20, 10, 10 });

        const auto button3 = CreateRef<GUI::Button>();
        button3->SetNormalColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        button3->SetNormalBackground(std::dynamic_pointer_cast<Texture2D>(buttonBg));
        button3->SetCornerRadius(12);

        const auto font2 = FontManager::Load("./Assets/Fonts/PlayfairDisplay-Regular.ttf");
        const auto txt = CreateRef<GUI::TextBlock>("Everyone, A pretty text block..");
        txt->SetFontSize(90.0f);
        txt->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        txt->SetFont(font2);

        const auto input = CreateRef<GUI::TextField>("Type here...");

        panel->AddChild(button)
            .SetAnchors(GUI::SAnchors::TopLeft())
            .SetPosition({ 10, 10 })
            .SetSize({ 100, 40 });

        panel->AddChild(button2)
            .SetAnchors(GUI::SAnchors::MiddleCenter())
            .SetAlignment({ 0.5f, 0.5f })
            .SetPosition({ 300, 0 });

        panel->AddChild(button3)
            .SetAnchors(GUI::SAnchors::MiddleCenter())
            .SetSize({ 200, 60 })
            .SetAlignment({ 0.5f, 2.0f });

        panel->AddChild(txt)
            .SetAnchors(GUI::SAnchors::BottomRight())
            .SetAlignment({ 1.0f, 1.0f })
            .SetSize({ 500, 90 })
            .SetPosition({ -500, -10 });

        panel->AddChild(input)
            .SetAnchors(GUI::SAnchors::BottomLeft())
            .SetAlignment({ 0.0f, 1.0f })
            .SetPosition({ 10, -10 });

        m_GUIManager->SetRoot(panel);
    }

    Application::~Application()
    {
        EE_PROFILE_ZONE_SCOPED()
        FontManager::Shutdown();
        Platform::Shutdown();
        m_Running = false;
    }

    constexpr auto EE_PROFILE_MAIN_LOOP = "Main Loop";
    constexpr auto EE_PROFILE_RUN_LOOP = "Run Loop";

    void Application::Run()
    {
        EE_PROFILE_ZONE_SCOPED()

        while (m_Running)
        {
            EE_PROFILE_ZONE_SCOPED_NAMED(EE_PROFILE_RUN_LOOP)

            const auto frameTime = m_Timer.GetLastFrameTime();

            m_Profiler.OnUpdate(frameTime); // TODO: Refactor to Update
            m_Window->OnUpdate(); // TODO: Refactor to Update

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
            m_GUIManager->Update(frameTime);

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

        ::InputManager::OnEvent(event);
        m_GUIManager->OnEvent(event);
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
