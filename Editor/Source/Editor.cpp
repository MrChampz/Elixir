#include "Editor.h"

#include <Engine/Core/Entrypoint.h>

#include "UI/Panels/ViewportPanel.h"
#include "UI/Panels/VFXPanel.h"

Editor::Editor()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Editor");

    m_EditorUI = CreateScope<EditorUI>(m_GUIManager.get());
    m_EditorUI->AddMenuItem("File");
    m_EditorUI->AddMenuItem("Edit");
    m_EditorUI->AddMenuItem("Assets");
    m_EditorUI->AddMenuItem("GameObject");
    m_EditorUI->AddMenuItem("Component");
    m_EditorUI->AddMenuItem("Window");
    m_EditorUI->AddMenuItem("Help");

    m_EditorUI->AddTab("Scene", false);
    m_EditorUI->AddTab("VFX", true);

    // Worked example of GUI::Manager's popup layer stack + GUI::ScrollBox: more items than
    // fit the dropdown's fixed height, so opening it actually exercises scrolling.
    m_EditorUI->AddDropdownMenu("Examples", {
        "Scene", "Prefab", "Material", "Texture 2D", "Cubemap",
        "Render Texture", "Animation", "Animator Controller",
        "Audio Mixer", "Physics Material", "Shader Graph",
    });

    m_EditorUI->AddPanel(CreateScope<ViewportPanel>(), 0);
    const auto extent = m_Window->GetFramebufferExtent();
    auto vfx = CreateScope<VFXPanel>(GetAetherManager(), *m_GraphicsContext,
        std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT),
        extent.Height > 0 ? static_cast<float>(extent.Width) / static_cast<float>(extent.Height) : 1.0f,
        [this](const std::string& status) { m_EditorUI->SetAssetBrowserStatus(status); });
    m_VFXPanel = vfx.get();
    m_EditorUI->AddPanel(std::move(vfx), 1);

    m_GraphicsContext->SetClearColor({ 0.015f, 0.025f, 0.06f, 1.0f });
}

Editor::~Editor() = default;

void Editor::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnGUI(frameTime);

    m_EditorUI->Update(frameTime);
}

void Editor::Render(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Render(frameTime);
    m_VFXPanel->Render(frameTime, m_SceneTarget);
}

void Editor::OnEvent(Event& event)
{
    Application::OnEvent(event);
    if (m_VFXPanel)
        m_VFXPanel->ProcessEvent(event, m_GUIManager->GetPopupCount() > 0);
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Editor();
}
