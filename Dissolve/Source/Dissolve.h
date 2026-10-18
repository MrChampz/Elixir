#pragma once

#include <Engine.h>

#include "Environment.h"

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
    // Submit the complete model to its renderer.
    void DrawGeometry();

    Scope<Environment> m_Environment;

    Scope<SkeletalMeshRenderer> m_SkeletalMeshRenderer;
    SkeletalMeshInstance m_MeshInstance;

    Scope<ArcBallCameraController> m_CameraController;
};
