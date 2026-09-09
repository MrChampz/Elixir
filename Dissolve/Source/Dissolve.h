#pragma once

#include <Engine.h>

class Dissolve final : public Elixir::Application
{
public:
    Dissolve();
    ~Dissolve() override;

    void OnGUI(Timestep frameTime) override;
    void Prepare(Timestep frameTime) override;
    void Render(Timestep frameTime) override;

    void OnEvent(Event& event) override;

private:
    void DrawGeometry();

    Scope<StaticMeshRenderer> m_StaticMeshRenderer;

    Scope<ArcBallCameraController> m_CameraController;
};
