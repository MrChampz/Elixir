#pragma once

#include <Engine.h>

#include <atomic>

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
     * @brief Apply the requested movement and advance the animation state machine.
     * @param deltaSeconds Elapsed time in seconds since the previous frame.
     */
    void UpdateAnimation(float deltaSeconds);

    /** @brief Configure directional locomotion driven by two numeric parameters. */
    void InitializeAnimationController();

    /** @brief Submit the complete model to its renderer. */
    void DrawGeometry();

    Scope<Environment> m_Environment;

    Scope<SkeletalMeshRenderer> m_SkeletalMeshRenderer;
    Scope<SkeletalMeshInstance> m_MeshInstance;

    Scope<AnimationControllerInstance> m_AnimationController;

    /** Input sampled on the application thread and consumed on the rendering thread. */
    std::atomic<float> m_RequestedForward = 0.0f;

    /** Evaluated forward coordinate published by the rendering thread for the
     * application-thread HUD. */
    std::atomic<float> m_AnimationForward = 0.0f;
    Ref<GUI::TextBlock> m_AnimationStatus;

    /** Persistent forward coordinate selected with the arrow keys on the application thread.
     */
    float m_ManualForward = 0.0f;
    std::atomic<float> m_RequestedStrafe = 0.0f;
    std::atomic<float> m_AnimationStrafe = 0.0f;
    float m_ManualStrafe = 0.0f;

    Scope<ArcBallCameraController> m_CameraController;
};
