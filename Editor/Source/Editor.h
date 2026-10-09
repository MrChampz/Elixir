#pragma once

#include <Engine.h>

#include "UI/EditorUI.h"

class VFXPanel;

/** @brief Hosts scene tools and the project VFX preview editor. */
class Editor final : public Elixir::Application
{
public:
    /** @brief Builds the Editor UI and its preview panels. */
    Editor();
    /** @brief Releases editor panels while the engine services are still available. */
    ~Editor() override;

    /** @brief Updates the active editor panel. */
    void OnGUI(Timestep frameTime) override;
    /** @brief Renders the selected effect into the application's HDR scene target. */
    void Render(Timestep frameTime) override;

    /** @brief Routes application events and VFX camera input. */
    void OnEvent(Event& event) override;

private:
    Scope<EditorUI> m_EditorUI;
    VFXPanel* m_VFXPanel = nullptr; // Owned by EditorUI.
};
