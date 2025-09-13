#include "Dissolve.h"

#include "Engine/Graphics/Shader/ShaderLoader.h"

#include <Engine/Core/Entrypoint.h>

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");

    const auto loader = CreateScope<ShaderLoader>(m_GraphicsContext.get());

    const auto shader = loader->LoadShader("./Shaders/", "Basic");
    shader->GetName();

    const auto shader1 = loader->LoadShader("./Shaders/", "Texture");
    shader1->GetName();
}

void Dissolve::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()

    Application::OnGUI(frameTime);
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
