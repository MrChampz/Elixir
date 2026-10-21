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
    Scope<SkeletalMeshInstance> m_ComparisonInstance;

    Scope<AnimationControllerInstance> m_AnimationController;
    Scope<AnimationControllerInstance> m_ComparisonController;
    Ref<const BlendSpace> m_LocomotionSpace;
    Ref<const AnimationSequence> m_WaveAnimation;
    Ref<const AnimationSequence> m_PunchAnimation;

    /** Application-thread action requests consumed by rendering-thread playback. */
    std::atomic<int> m_ActionRequest = 0;
    std::atomic<bool> m_LayerEnabled = true;
    std::atomic<float> m_LayerWeight = 1.0f;
    std::atomic<uint64_t> m_ActionEvents = 0;
    bool m_WaveKeyPressed = false;
    bool m_PunchKeyPressed = false;
    bool m_LayerKeyPressed = false;


    /** Common phase of the reference clip's left-foot support marker. */
    float m_ContactPhase = 0.0f;

    /** Footstep feedback published by rendering-thread playback for the HUD. */
    std::atomic<uint64_t> m_SynchronizedSteps = 0;
    std::atomic<uint64_t> m_ComparisonSteps = 0;
    std::atomic<int> m_LastFoot = 0;
    std::atomic<float> m_AnimationPhase = 0.0f;
    std::atomic<bool> m_AnimationPaused = false;
    std::atomic<bool> m_StepAnimation = false;
    std::atomic<bool> m_SeekContact = false;
    bool m_PauseKeyPressed = false;
    bool m_StepKeyPressed = false;
    bool m_ContactKeyPressed = false;

    /** Input sampled on the application thread and consumed on the rendering thread. */
    std::atomic<float> m_RequestedForward = 0.0f;

    /** Evaluated forward coordinate published by the rendering thread for the
     * application-thread HUD. */
    std::atomic<float> m_AnimationForward = 0.0f;
    Ref<GUI::TextBlock> m_AnimationStatus;

    /** Persistent forward coordinate selected with the arrow keys on the application thread.
     */
    float m_ManualForward = -0.75f;
    std::atomic<float> m_RequestedStrafe = 0.0f;
    std::atomic<float> m_AnimationStrafe = 0.0f;
    float m_ManualStrafe = -1.5f;

    Scope<ArcBallCameraController> m_CameraController;
};
