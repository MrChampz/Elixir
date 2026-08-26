#include "Editor.h"

#include <Engine/Core/Entrypoint.h>

#include "UI/Panels/ViewportPanel.h"

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

    m_EditorUI->AddTab("Scene", true);
    m_EditorUI->AddTab("Game", false);

    // Worked example of GUI::Manager's popup layer stack + GUI::ScrollBox: more items than
    // fit the dropdown's fixed height, so opening it actually exercises scrolling.
    m_EditorUI->AddDropdownMenu("Examples", {
        "Scene", "Prefab", "Material", "Texture 2D", "Cubemap",
        "Render Texture", "Animation", "Animator Controller",
        "Audio Mixer", "Physics Material", "Shader Graph",
    });

    m_EditorUI->AddPanel(CreateScope<ViewportPanel>());

    m_GraphicsContext->SetClearColor({ 0.49f, 0.65f, 0.98f, 1.0f });
}

Editor::~Editor() = default;

void Editor::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnGUI(frameTime);

    m_EditorUI->Update(frameTime);
}

void Editor::OnRender(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnRender(frameTime);
    m_GraphicsContext->Clear();
}

void Editor::OnEvent(Event& event)
{
    Application::OnEvent(event);
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Editor();
}
