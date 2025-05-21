#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Core/Executor.h>

struct ThreadArgs
{
    std::string Name;
};

void TaskEntrypoint(ftl::TaskScheduler* scheduler, void* args)
{
    EE_PROFILE_ZONE_SCOPED()

    auto* threadArgs = static_cast<ThreadArgs*>(args);

    std::ostringstream os;
    os << std::this_thread::get_id();
    EE_CORE_INFO("Task routine in thread {0} - {1}", os.str(), threadArgs->Name)
}

void* RenderEntrypoint(void* args)
{
    EE_PROFILE_ZONE_SCOPED()

    auto* threadArgs = static_cast<ThreadArgs*>(args);

    std::ostringstream os;
    os << std::this_thread::get_id();
    EE_CORE_INFO("Render routine in thread {0} - {1}", os.str(), threadArgs->Name)
    return nullptr;
}

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");

    auto renderArgs = ThreadArgs{};
    renderArgs.Name = "Render";

    auto logicArgs = ThreadArgs{};
    logicArgs.Name = "Logic";

    auto thread = Executor::CreateThread(1048576, RenderEntrypoint, &renderArgs, "RenderThread");

    m_Executor = CreateScope<Executor>();
    m_Executor->Init();

    std::ostringstream os;
    os << std::this_thread::get_id();
    EE_CORE_INFO("Init in thread {0}", os.str())

    WaitGroup wg(m_Executor.get());
    m_Executor->AddTask({ TaskEntrypoint, &logicArgs }, TaskPriority::Normal, &wg);
    wg.Wait();

    Executor::JoinThread(thread);
}

void Dissolve::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()

    Application::OnGUI(frameTime);

    if (InputManager::IsGamepadConnected(EE_JOYSTICK_1))
    {
        const float axis = InputManager::GetGamepadAxis(EE_JOYSTICK_1, EE_GAMEPAD_AXIS_LEFT_Y);
        if (axis != 0.0)
        {
            const char* name = InputManager::GetGamepadName(EE_JOYSTICK_1);
            EE_INFO("Axis Y ({0}) in {1}", axis, name)
        }
    }
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
