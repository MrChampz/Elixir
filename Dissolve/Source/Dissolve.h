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

    /** @brief Create a chest recoil sequence relative to the mesh reference pose. */
    Ref<const AnimationSequence> CreateRecoilAnimation();

    /**
     * @brief Build a demonstration timeline with three attacks and a recovery range.
     * @param attack Source punch sequence sampled at 60 Hz.
     * @param recovery Source wave sequence used for the final return pose.
     */
    Ref<const AnimationSequence> CreateComboAnimation(
        const Ref<const AnimationSequence>& attack,
        const Ref<const AnimationSequence>& recovery
    );

    /** @brief Create a rigid baton and configure its two hand-grip references. */
    void InitializeSocketAttachment();

    /** @brief Submit the complete model to its renderer. */
    void DrawGeometry();

    Scope<Environment> m_Environment;

    Scope<SkeletalMeshRenderer> m_SkeletalMeshRenderer;
    Scope<GeometryPool> m_AttachmentGeometryPool;
    Scope<StaticMeshRenderer> m_AttachmentRenderer;
    Ref<const StaticMesh> m_AttachmentMesh;
    Scope<SkeletalMeshInstance> m_MeshInstance;
    Scope<SkeletalMeshInstance> m_ComparisonInstance;

    Scope<AnimationControllerInstance> m_AnimationController;
    Scope<AnimationControllerInstance> m_ComparisonController;
    Ref<const BlendSpace> m_LocomotionSpace;
    Ref<const AnimationMontage> m_PunchMontage;
    Ref<const AnimationMontage> m_DodgeMontage;

    /** Root movement is applied by the demo on the rendering thread. */
    glm::mat4 m_CharacterMotion{1};
    std::atomic<bool> m_DodgeRequested = false;
    std::atomic<bool> m_ResetMotion = false;
    std::atomic<float> m_RootMotionDistance = 0;

    /** Last composed support-hand curve, published to the GUI thread. */
    std::atomic<float> m_SupportHandIKWeight = 1;
    bool m_DodgeKeyPressed = false;
    bool m_ResetMotionKeyPressed = false;

    /** Application-thread action requests consumed by rendering-thread playback. */
    std::atomic<int> m_ActionRequest = 0;
    std::atomic<bool> m_LayerEnabled = true;
    std::atomic<float> m_LayerWeight = 1.0f;
    std::atomic<uint64_t> m_ActionEvents = 0;
    std::atomic<int> m_ComboSection = 0;
    std::atomic<bool> m_HitboxActive = false;
    std::atomic<uint64_t> m_NotifyStateUpdates = 0;
    std::atomic<uint64_t> m_CancelledNotifyStates = 0;
    Ref<GUI::TextBlock> m_NotifyStateStatus;
    bool m_WaveKeyPressed = false;
    bool m_PunchKeyPressed = false;
    bool m_LayerKeyPressed = false;

    /** Recoil controls published to the rendering thread; strength stays in [0, 1]. */
    std::atomic<bool> m_IKEnabled = true;
    bool m_IKKeyPressed = false;
    std::atomic<bool> m_IKRotationEnabled = true;
    bool m_IKRotationKeyPressed = false;
    /** Aligns the baton with skeleton +X through the imported wrist-to-weapon offset. */
    glm::quat m_IKDriverRotation{1, 0, 0, 0};
    /** Left-wrist offset from its grip, expressed in SupportGrip coordinates. */
    SBoneTransform m_IKSupportOffset;
    uint32_t m_IKDriverBone = 0;
    uint32_t m_IKEndBone = 0;
    const glm::vec3 m_IKTarget{-0.2f, 1.02f, -0.28f};
    std::atomic<bool> m_RecoilEnabled = true;
    std::atomic<float> m_RecoilStrength = 1.0f;
    std::atomic<uint64_t> m_RecoilEvents = 0;
    bool m_RecoilKeyPressed = false;


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
