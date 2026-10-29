#pragma once

#include <Engine.h>

#include "UI/EditorUI.h"

class Editor final : public Elixir::Application
{
public:
    Editor();
    ~Editor() override;

    void OnGUI(Timestep frameTime) override;
    /**
     * @brief Clear the editor world target on the rendering thread.
     * @param frameTime Elapsed application frame time.
     */
    void Render(Timestep frameTime) override;

    void OnEvent(Event& event) override;

private:
    Scope<EditorUI> m_EditorUI;
};
