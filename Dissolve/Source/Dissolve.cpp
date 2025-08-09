#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Core/Executor.h>
#include <Engine/Renderer/RenderGraph.h>
using namespace Elixir::Renderer;

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Executor = CreateScope<Executor>();
    m_Executor->Init();

    m_Window->SetTitle("Dissolve");

    RenderGraph rg(m_Executor.get());

    auto tex1 = rg.CreateResource({ ERGResourceType::Texture, 900, 600 });
    auto tex2 = rg.CreateResource({ ERGResourceType::Texture, 900, 600 });

    rg.AddPass("Pass 1", {}, { tex1 }, []()
    {
       std::cout << "Pass 1 - " << std::this_thread::get_id() << std::endl;
    });

    rg.AddPass("Pass 2", { tex2 }, {}, []()
    {
       std::cout << "Pass 2 - " << std::this_thread::get_id() << std::endl;
    });

    rg.AddPass("Pass 3", { tex1 }, { tex2 }, []()
    {
       std::cout << "Pass 3 - " << std::this_thread::get_id() << std::endl;
    });

    rg.Compile();
    rg.Execute();
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
