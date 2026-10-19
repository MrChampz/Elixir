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
    /**
     * @brief Advance the animation demo and cross-fade to the next clip every two seconds.
     * @param deltaSeconds Elapsed time in seconds since the previous frame.
     */
    void UpdateAnimation(float deltaSeconds);

    /** @brief Submit the complete model to its renderer. */
    void DrawGeometry();

    Scope<Environment> m_Environment;

    Scope<SkeletalMeshRenderer> m_SkeletalMeshRenderer;
    SkeletalMeshInstance m_MeshInstance;

    size_t m_AnimationIndex = 0;
    float m_AnimationTime = 0.0f;

    Scope<ArcBallCameraController> m_CameraController;
};
