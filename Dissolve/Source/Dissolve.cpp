#include <Engine/Animation/Pose.h>
#include "Dissolve.h"

#include "Engine/Materials/Nodes/Color.h"
#include "Engine/Materials/Nodes/Parameter.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Aether/Manager.h>
#include <Engine/GUI/Canvas.h>
#include <Engine/Materials/Nodes/Constant.h>

#include <glm/gtc/matrix_transform.hpp>

#include <Engine/Materials/MaterialGraph.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Materials/MaterialRegistry.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/TextureSample.h>
#include <Engine/Materials/Nodes/Panner.h>

using namespace Elixir::Materials::Nodes;

namespace
{
    /** Time spent fading the held final punch pose back to the continuing source. */
    constexpr float ComboRecoveryDuration = 0.3f;

    /** Identifies the foot associated with a locomotion event. */
    enum class EFoot : uint8_t { Left = 1, Right = 2 };

    /** Carries application-specific footstep data without engine-owned gameplay types. */
    struct SFootstepNotifyPayload
    {
        EFoot Foot;
    };

    /** Supplies game-defined damage settings for the demonstration's active attack window. */
    struct SAttackWindowPayload
    {
        uint32_t Damage = 12;
    };
}

std::array<Ref<Aether::System>, 2> m_ParticleSystems;
std::array<Ref<Aether::SystemInstance>, 2> m_ParticleSystemInstances;

Ref<Material> graphMaterial;

Ref<SkeletalMesh> mesh;
SMeshLighting s_LightingSetup;

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");
    const auto drawExtent = m_Window->GetFramebufferExtent();
    const auto aspectRatio = (float)drawExtent.Width / (float)drawExtent.Height;
    // m_CameraController = CreateScope<SplineCameraController>(aspectRatio);
    // m_CameraController->AddKeyframe({ { 0.0f, 0.5f, 5.0f }, { 0.0f, 0.0f, 0.0f }, 60.0f });
    // m_CameraController->AddKeyframe({ { 1.0f, 0.3f, 2.5f }, { 0.0f, 0.0f, 0.0f }, 50.0f });
    // m_CameraController->AddKeyframe({ { 0.0f, 0.0f, 1.5f }, { 0.0f, 0.0f, 0.0f }, 40.0f });
    // m_CameraController->SetDuration(4.0f);
    // m_CameraController->SetLooping(true);
    // m_CameraController->Play();
    m_CameraController = CreateScope<ArcBallCameraController>(60.0f, aspectRatio);
    const auto tex = TextureLoader::Load("./Assets/Bricks.png");

    m_Environment = Environment::Load(
        *m_GraphicsContext,
        "./Assets/Textures/white_home_studio_2k.hdr"
    );
    EE_CORE_ASSERT(m_Environment, "Could not load the Environment.")

    m_SkeletalMeshRenderer = CreateScope<SkeletalMeshRenderer>(
        *m_GraphicsContext,
        GetMaterialSystem(),
        *m_ShaderLoader
    );

    s_LightingSetup = {
        .Environment = {
            .Environment = m_Environment->GetEnvironment(),
            .Irradiance = m_Environment->GetIrradiance(),
            .Prefiltered = m_Environment->GetPrefiltered(),
            .Sampler = m_Environment->GetSampler(),
            .Intensity = 1.5f,
            .MaxLod = Environment::GetMaxLod(),
        },
        .DirectionalLightDirection = glm::normalize(glm::vec3{ -0.5f, 0.65f, -0.55f }),
        .DirectionalLightColor = { 1.0f, 0.96f, 0.9f },
        .DirectionalLightIntensity = 2.2f,
        .DebugView = ESurfaceDebugView::Composite,
    };

    m_ParticleSystems[0] = GetAetherManager().LoadEffect("./Assets/VFX/FireAndFireworks.json");
    EE_CORE_ASSERT(m_ParticleSystems[0], "Could not resolve FireAndFireworks effect.")

    m_ParticleSystems[1] = GetAetherManager().LoadEffect("./Assets/VFX/RibbonVortex.json");
    EE_CORE_ASSERT(m_ParticleSystems[1], "Could not resolve RibbonVortex effect.")

    {
        MaterialGraph graph;

        graphMaterial = CreateRef<Material>("DissolveGraph", EMaterialUsage::Particle);
        EE_CORE_ASSERT(
            graphMaterial->SetBlendMode(EMaterialBlendMode::Translucent),
            "Dissolve graph material must enable translucent blending."
        )

        EE_CORE_ASSERT(
            graphMaterial->DefineParameter(
                "Tint",
                {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float4,
                    .DefaultValue = SMaterialParameter::MakeVector({1.0f, 0.5f, 0.2f, 1.0f}),
                }
            ),
            ""
        )

        EE_CORE_ASSERT(
            graphMaterial->DefineParameter(
                "Albedo",
                {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(tex),
                }
            ),
            ""
        )

        const auto albedo = graph.AddNode<TextureSample>("Albedo");
        const auto tint = graph.AddNode<Parameter>("Tint", EMaterialValueType::Float4);

        const auto baseColor = graph.AddNode<Multiply>();
        graph.Connect(albedo, baseColor, 0);
        graph.Connect(tint, baseColor, 0);

        graph.SetChannel(EMaterialChannel::BaseColor, baseColor);

        graphMaterial->SetGraph(std::move(graph));
        EE_CORE_ASSERT(
            GetMaterialRegistry().Register(graphMaterial),
            "GraphMaterial must be unique."
        )

        auto instance = graphMaterial->CreateInstance();
        EE_CORE_ASSERT(
            instance->SetVector("Tint", {1.0f, 0.35f, 0.1f, 1.0f}),
            "Dissolve graph material tint override must match its schema."
        )

        if (auto* emitter = m_ParticleSystems[0]->FindEmitter("FlameCore"))
        {
            emitter->SetMaterial(instance);
            EE_CORE_INFO("Published graph material to the FlameCore particle emitter.")
        }
        else
        {
            EE_CORE_ERROR("Dissolve particle emitter 'FlameCore' was not found.")
        }

        InitializeSocketCharacter();
    }

    {
        MaterialGraph graph1;

        const auto ribbonMaterial = CreateRef<Material>(
            "RibbonEnergy",
            EMaterialUsage::Particle
        );
        EE_CORE_ASSERT(
            ribbonMaterial->SetBlendMode(EMaterialBlendMode::Translucent),
            "Ribbon material must enable translucent blending."
        )

        EE_CORE_ASSERT(
            ribbonMaterial->DefineParameter(
                "Tint",
                {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float4,
                    .DefaultValue = SMaterialParameter::MakeVector({1.0f, 1.0f, 1.0f, 1.0f}),
                }
            ),
            ""
        )

        const auto tint = graph1.AddNode<Parameter>("Tint", EMaterialValueType::Float4);

        const auto color = graph1.AddNode<Nodes::Color>();

        const auto multiply = graph1.AddNode<Multiply>();
        graph1.Connect(tint, multiply, 0);
        graph1.Connect(color, multiply, 1);
        graph1.SetChannel(EMaterialChannel::BaseColor, multiply);

        // const auto glow = graph1.AddNode<Parameter>("Glow", EMaterialValueType::Float4);
        // graph1.SetChannel(EMaterialChannel::Emissive, glow);

        ribbonMaterial->SetGraph(std::move(graph1));
        EE_CORE_ASSERT(
            GetMaterialRegistry().Register(ribbonMaterial),
            "RibbonEnergy must be unique."
        )

        if (auto* emitter = m_ParticleSystems[1]->FindEmitter("PathRibbon"))
        {
            // emitter->SetMaterial(ribbonMaterial);
            EE_CORE_INFO("Published graph material to the PathRibbon particle emitter.")
        }
    }

    m_ParticleSystemInstances[0] = m_ParticleSystems[0]->CreateInstance();
    m_ParticleSystemInstances[1] = m_ParticleSystems[1]->CreateInstance();
    EE_CORE_ASSERT(m_ParticleSystemInstances[0], "Could not create FireAndFireworks instance.")
    EE_CORE_ASSERT(m_ParticleSystemInstances[1], "Could not create RibbonVortex instance.")

    auto& aether = GetAetherManager();

    bool submitted = aether.Add(m_ParticleSystemInstances[0]);
    EE_CORE_ASSERT(submitted, "The particle system instance was submitted more than once.")

    submitted = aether.Add(m_ParticleSystemInstances[1]);
    EE_CORE_ASSERT(submitted, "The particle system instance was submitted more than once.")

    const auto panel = CreateRef<GUI::Canvas>();
    const auto instructions = CreateRef<GUI::TextBlock>(
        "WASD/Arrows: direction | 1/2/3: movement presets | Shift/R: run"
    );
    instructions->SetFontSize(20.0f);
    panel->AddChild(instructions)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 20})
        .SetSize({1100, 30});
    const auto playbackInstructions = CreateRef<GUI::TextBlock>(
        "P: pause | N: frame | F: contact | Space: idle | U: recoil on/off | I/O: recoil 0.5/1"
    );
    playbackInstructions->SetFontSize(18.0f);
    panel->AddChild(playbackInstructions)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 50})
        .SetSize({1000, 30});
    for (const auto& [position, caption] : std::array{
        std::pair{0.25f, "ROOT LOCKED"}, std::pair{0.75f, "ROOT MOTION + IK"}
    })
    {
        const auto label = CreateRef<GUI::TextBlock>(caption);
        label->SetFontSize(24.0f);
        panel->AddChild(label)
            .SetAnchors({position, 0, position, 0})
            .SetAlignment({0.5f, 0})
            .SetPosition({0, 190})
            .SetSize({260, 35});
    }
    const auto layerInstructions = CreateRef<GUI::TextBlock>(
        "J: recovery | K: start/chain combo | L: action on/off | B: dodge | "
        "V: reset | G: IK | H: IK rotation"
    );
    layerInstructions->SetFontSize(18.0f);
    panel->AddChild(layerInstructions)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 80})
        .SetSize({1100, 30});
    m_AnimationStatus = CreateRef<GUI::TextBlock>("KayKit animation layers");
    m_AnimationStatus->SetFontSize(20.0f);
    panel->AddChild(m_AnimationStatus)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 110})
        .SetSize({1100, 30});
    m_NotifyStatus = CreateRef<GUI::TextBlock>("Hitbox OFF | Window updates 0 | Cancelled 0");
    m_NotifyStatus->SetFontSize(18.0f);
    panel->AddChild(m_NotifyStatus)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 140})
        .SetSize({1100, 30});
    m_GUIManager->SetRoot(panel);

    m_GraphicsContext->SetClearColor({0.015f, 0.025f, 0.06f, 1.0f});

    // Start the demo clock after loading assets so the first animation remains visible.
    m_Timer = Timer{};
}

Dissolve::~Dissolve()
{
    m_AttachmentRenderer.reset();
    m_AttachmentMesh.reset();
    m_AttachmentGeometryPool.reset();
    m_SkeletalMeshRenderer.reset();
    mesh.reset();
    s_LightingSetup = {};
    graphMaterial.reset();
    m_ParticleSystemInstances = {};
    m_ParticleSystems = {};
}

void Dissolve::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnGUI(frameTime);
    if (m_AnimationStatus)
        m_AnimationStatus->SetText(
            std::format(
                "Phase {:.2f} | {} | Steps {} / {} | {} | Actions {} | Recoil {:.2f} | Shots {} | Root {:.2f}",
                m_AnimationPhase.load(std::memory_order_relaxed),
                m_AnimationPaused.load(std::memory_order_relaxed) ? "PAUSED" : "PLAYING",
                m_ComparisonSteps.load(std::memory_order_relaxed),
                m_SynchronizedSteps.load(std::memory_order_relaxed),
                std::array{"WAVE", "ATTACK 1", "ATTACK 2", "ATTACK 3", "RECOVERY"}
                    [m_ComboSection.load(std::memory_order_relaxed)],
                m_ActionEvents.load(std::memory_order_relaxed),
                m_RecoilEnabled.load(std::memory_order_relaxed)
                    ? m_RecoilStrength.load(std::memory_order_relaxed) : 0.0f,
                m_RecoilEvents.load(std::memory_order_relaxed),
                m_RootMotionDistance.load(std::memory_order_relaxed)
            )
        );
    if (m_NotifyStatus)
        m_NotifyStatus->SetText(std::format(
            "Hitbox {} | Window updates {} | Cancelled {} | SupportHandIK {:.2f}",
            m_HitboxActive.load(std::memory_order_relaxed) ? "ON" : "OFF",
            m_NotifyUpdates.load(std::memory_order_relaxed),
            m_CancelledNotifies.load(std::memory_order_relaxed),
            m_SupportHandIKWeight.load(std::memory_order_relaxed)
        ));
}

void Dissolve::Prepare(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Prepare(frameTime);

    const auto dodge = InputManager::IsKeyPressed(EE_KEY_B);
    if (dodge && !m_DodgeKeyPressed)
        m_DodgeRequested.store(true, std::memory_order_relaxed);
    m_DodgeKeyPressed = dodge;
    const auto resetMotion = InputManager::IsKeyPressed(EE_KEY_V);
    if (resetMotion && !m_ResetMotionKeyPressed)
        m_ResetMotion.store(true, std::memory_order_relaxed);
    m_ResetMotionKeyPressed = resetMotion;

    const auto ik = InputManager::IsKeyPressed(EE_KEY_G);
    if (ik && !m_IKKeyPressed)
    {
        m_IKEnabled.store(
            !m_IKEnabled.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
    }
    m_IKKeyPressed = ik;
    const auto ikRotation = InputManager::IsKeyPressed(EE_KEY_H);
    if (ikRotation && !m_IKRotationKeyPressed)
    {
        m_IKRotationEnabled.store(
            !m_IKRotationEnabled.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
    }
    m_IKRotationKeyPressed = ikRotation;

    const auto recoil = InputManager::IsKeyPressed(EE_KEY_U);
    if (recoil && !m_RecoilKeyPressed)
        m_RecoilEnabled.store(!m_RecoilEnabled.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    m_RecoilKeyPressed = recoil;
    if (InputManager::IsKeyPressed(EE_KEY_I))
        m_RecoilStrength.store(0.5f, std::memory_order_relaxed);
    if (InputManager::IsKeyPressed(EE_KEY_O))
        m_RecoilStrength.store(1.0f, std::memory_order_relaxed);

    const auto wave = InputManager::IsKeyPressed(EE_KEY_J);
    const auto punch = InputManager::IsKeyPressed(EE_KEY_K);
    const auto toggleLayer = InputManager::IsKeyPressed(EE_KEY_L);
    if (wave && !m_WaveKeyPressed)
        m_ActionRequest.store(1, std::memory_order_relaxed);
    if (punch && !m_PunchKeyPressed)
        m_ActionRequest.store(2, std::memory_order_relaxed);
    if (toggleLayer && !m_LayerKeyPressed)
        m_LayerEnabled.store(!m_LayerEnabled.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    m_WaveKeyPressed = wave;
    m_PunchKeyPressed = punch;
    m_LayerKeyPressed = toggleLayer;

    const auto pause = InputManager::IsKeyPressed(EE_KEY_P);
    if (pause && !m_PauseKeyPressed)
        m_AnimationPaused.store(!m_AnimationPaused.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    m_PauseKeyPressed = pause;
    const auto stepFrame = InputManager::IsKeyPressed(EE_KEY_N);
    if (stepFrame && !m_StepKeyPressed)
    {
        m_AnimationPaused.store(true, std::memory_order_relaxed);
        m_StepAnimation.store(true, std::memory_order_relaxed);
    }
    m_StepKeyPressed = stepFrame;
    const auto contact = InputManager::IsKeyPressed(EE_KEY_F);
    if (contact && !m_ContactKeyPressed)
    {
        m_AnimationPaused.store(true, std::memory_order_relaxed);
        m_SeekContact.store(true, std::memory_order_relaxed);
    }
    m_ContactKeyPressed = contact;
    if (InputManager::IsKeyPressed(EE_KEY_1))
    {
        m_ManualStrafe = 0.0f;
        m_ManualForward = 2.25f;
    }
    if (InputManager::IsKeyPressed(EE_KEY_2))
    {
        m_ManualStrafe = 1.0f;
        m_ManualForward = 1.0f;
    }
    if (InputManager::IsKeyPressed(EE_KEY_3))
    {
        m_ManualStrafe = -1.5f;
        m_ManualForward = -0.75f;
    }

    const auto forward = float(InputManager::IsKeyPressed(EE_KEY_UP)) -
                         float(InputManager::IsKeyPressed(EE_KEY_DOWN));
    const auto lateral = float(InputManager::IsKeyPressed(EE_KEY_RIGHT)) -
                         float(InputManager::IsKeyPressed(EE_KEY_LEFT));
    const auto step = 2.0f * frameTime.GetSeconds();
    m_ManualForward = std::clamp(m_ManualForward + forward * step, -1.5f, 3.0f);
    m_ManualStrafe = std::clamp(m_ManualStrafe + lateral * step, -3.0f, 3.0f);
    if (InputManager::IsKeyPressed(EE_KEY_SPACE))
        m_ManualForward = m_ManualStrafe = 0.0f;

    const auto shift = InputManager::IsKeyPressed(EE_KEY_LEFT_SHIFT) ||
                       InputManager::IsKeyPressed(EE_KEY_RIGHT_SHIFT) ||
                       InputManager::IsKeyPressed(EE_KEY_R);
    const auto movement = glm::vec2(
        float(InputManager::IsKeyPressed(EE_KEY_D)) -
            float(InputManager::IsKeyPressed(EE_KEY_A)),
        float(InputManager::IsKeyPressed(EE_KEY_W)) -
            float(InputManager::IsKeyPressed(EE_KEY_S))
    );
    const auto active = glm::dot(movement, movement) > 0.0f;
    const auto requested = active ? glm::normalize(movement) * (shift ? 3.0f : 1.5f)
                                  : glm::vec2(m_ManualStrafe, m_ManualForward);
    m_RequestedForward.store(requested.y, std::memory_order_relaxed);
    m_RequestedStrafe.store(requested.x, std::memory_order_relaxed);
}

void Dissolve::Render(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Render(frameTime);

    m_CameraController->Update(frameTime);
    auto& aether = GetAetherManager();
    aether.BeginFrame(frameTime);

    m_GraphicsContext->Clear();

    UpdateAnimation(frameTime.GetSeconds());
    m_SkeletalMeshRenderer->BeginFrame(m_CameraController->GetCamera(), s_LightingSetup);
    if (m_AttachmentRenderer)
        m_AttachmentRenderer->BeginFrame(m_CameraController->GetCamera(), s_LightingSetup);
    DrawGeometry();
    m_SkeletalMeshRenderer->EndFrame();
    if (m_AttachmentRenderer)
        m_AttachmentRenderer->EndFrame();

    // Keep particle effects out of the locomotion comparison.
}

void Dissolve::OnEvent(Event& event)
{
    Application::OnEvent(event);
    m_CameraController->ProcessEvent(event);
}

void Dissolve::InitializeAnimationGraph()
{
    const auto& animations = m_MeshInstance->GetMesh()->GetAnimations();
    const auto findAnimation = [&](const std::string_view name) -> Ref<const Sequence>
    {
        const auto found = std::ranges::find_if(
            animations,
            [&](const auto& animation)
            {
                return animation->GetName() == name;
            }
        );
        return found == animations.end() ? nullptr
            : (*found)->WithCurves({{"SupportHandIK", {{0, 1}}}});
    };

    const auto idle = findAnimation("Idle_A");
    const auto walkSource = findAnimation("Walking_A");
    const auto runSource = findAnimation("Running_A");
    const auto backwardSource = findAnimation("Walking_Backwards");
    const auto leftSource = findAnimation("Running_Strafe_Left");
    const auto rightSource = findAnimation("Running_Strafe_Right");
    if (!idle || !walkSource || !runSource || !backwardSource || !leftSource || !rightSource)
    {
        EE_CORE_ERROR("KayKit locomotion requires all six directional animations.")
        return;
    }
    const auto addFootsteps = [](
        const Ref<const Sequence>& clip,
        const float leftTime,
        const float rightTime
    )
    {
        return clip->WithNotifies({
            { "Footstep", leftTime, 0, SFootstepNotifyPayload{ EFoot::Left } },
            { "Footstep", rightTime, 0, SFootstepNotifyPayload{ EFoot::Right } },
        });
    };

    const auto walk = addFootsteps(walkSource, 0.3217778f, 0.8622223f);
    const auto run = addFootsteps(runSource, 0.1146667f, 0.5146667f);
    const auto backward = addFootsteps(backwardSource, 0.8160000f, 0.2826667f);
    const auto left = addFootsteps(leftSource, 0.1493333f, 0.5613333f);
    const auto right = addFootsteps(rightSource, 0.1573333f, 0.5453333f);
    if (!walk || !run || !backward || !left || !right)
        return;

    // Authored at the lowest ankle position during full support; see the comparison notes.
    const auto syncWalk = walk->WithSyncMarkers({
        { "LeftFootPlant", 0.3217778f },
        { "RightFootPlant", 0.8622223f },
    });
    const auto syncRun = run->WithSyncMarkers({
        { "LeftFootPlant", 0.1146667f },
        { "RightFootPlant", 0.5146667f },
    });
    const auto syncBackward = backward->WithSyncMarkers({
        { "LeftFootPlant", 0.8160000f },
        { "RightFootPlant", 0.2826667f },
    });
    const auto syncLeft = left->WithSyncMarkers({
        { "LeftFootPlant", 0.1493333f },
        { "RightFootPlant", 0.5613333f },
    });
    const auto syncRight = right->WithSyncMarkers({
        { "LeftFootPlant", 0.1573333f },
        { "RightFootPlant", 0.5453333f },
    });
    if (!syncWalk || !syncRun || !syncBackward || !syncLeft || !syncRight)
        return;

    const auto blendSpace = BlendSpace2D::Create(
        "KayKitSynchronized",
        {
            { { 0, 0 }, idle },
            { { 0, 1.5f }, syncWalk },
            { { 0, 3 }, syncRun },
            { { 0, -1.5f }, syncBackward },
            { { -3, 0 }, syncLeft },
            { { 3, 0 }, syncRight },
        }
    );
    if (!blendSpace)
        return;

    const auto wave = findAnimation("Waving");
    const auto punch = findAnimation("Melee_Unarmed_Attack_Punch_A");
    const auto upperBody = BoneMask::FromBone("UpperBody", mesh->GetSkeleton(), "spine");
    const auto recoilMask = BoneMask::FromBone("Recoil", mesh->GetSkeleton(), "chest");
    const auto recoilAnimation = CreateRecoilAnimation();
    if (!wave || !punch || !upperBody || !recoilMask || !recoilAnimation)
        return;

    const auto waveAnimation = wave->WithNotifies({{ "WavePeak", wave->GetDuration() * 0.5f }});
    const auto punchAnimation = punch->WithNotifies({{ "PunchHit", punch->GetDuration() * 0.45f }});
    if (!waveAnimation || !punchAnimation)
        return;

    const auto dodgeLeft = findAnimation("Dodge_Left");
    if (!dodgeLeft)
        return;

    const auto comboAnimation = CreateComboAnimation(punchAnimation);
    if (!comboAnimation)
        return;
    const auto attackDuration = punchAnimation->GetDuration();
    const auto punchMontage = AnimationMontage::Create({
        .Name = "PunchCombo",
        .Sequence = comboAnimation,
        .BlendIn = 0.1f,
        .BlendOut = ComboRecoveryDuration,
        .Sections = {
            { "Attack1", 0, "Recovery" },
            { "Attack2", attackDuration, "Recovery" },
            { "Attack3", attackDuration * 2, "Recovery" },
            { "Recovery", attackDuration * 3 }
        }
    });
    const auto dodgeMontage = AnimationMontage::Create({
        .Name = "Dodge", .Sequence = dodgeLeft, .BlendIn = 0.05f, .BlendOut = 0.05f
    });
    if (!punchMontage || !dodgeMontage)
        return;

    const auto graph = AnimationGraph::Create({
        .Name = "KayKitCharacter",
        .Parameters = {
            { "Strafe", 0.0f },
            { "Forward", 0.0f },
        },
        .Output = BlendNode::Create("CharacterComposition", {
            {
                .Name = "Locomotion",
                .Node = BlendSpaceNode::Create(blendSpace),
                .InputParameters = { "Strafe", "Forward" }
            },
            { .Name = "UpperBody", .Mask = upperBody, .Node = SequenceNode::Create(waveAnimation) },
            {
                .Name = "Recoil",
                .Mask = recoilMask,
                .BlendMode = EAnimationLayerBlendMode::Additive,
                .Node = SequenceNode::Create(recoilAnimation)
            },
        }),
    });

    if (!graph) return;

    auto animation = graph->CreateAnimation(m_MeshInstance->GetMesh()->GetData().ReferencePose);
    auto comparisonAnimation = graph->CreateAnimation(
        m_ComparisonInstance->GetMesh()->GetData().ReferencePose);
    if (!animation || !comparisonAnimation) return;

    m_Animation = std::move(animation);
    m_ComparisonAnimation = std::move(comparisonAnimation);

    auto& instance = *m_Animation;
    auto& comparison = *m_ComparisonAnimation;

    if (!m_Animation->SetRootMotionEnabled(true, "root") ||
        !m_ComparisonAnimation->SetRootMotionEnabled(true, "root") ||
        !instance.Update(0.0f) || !comparison.Update(0.0f) ||
        !m_ComparisonAnimation->GetLayer("Recoil")->SetWeight(0.0f))
        return;

    m_LocomotionSpace = blendSpace;
    m_PunchMontage = punchMontage;
    m_DodgeMontage = dodgeMontage;
    m_ContactPhase = syncLeft->GetSyncMarkers().front().Time / syncLeft->GetDuration();
    m_AnimationGraph = graph;
    EE_CORE_INFO("KayKit sections: K starts or chains attacks, J jumps to recovery, B dodges.")
}

Ref<const Sequence> Dissolve::CreateComboAnimation(
    const Ref<const Sequence>& attack
)
{
    const auto& reference = m_MeshInstance->GetMesh()->GetData().ReferencePose;
    std::vector<SBoneAnimationTrack> tracks(reference.LocalTransforms.size());
    for (size_t i = 0; i < tracks.size(); ++i)
        tracks[i].BoneIndex = uint32_t(i);

    const auto attackDuration = attack->GetDuration();
    const auto recoveryStart = attackDuration * 3;
    for (int section = 0; section < 4; ++section)
    {
        const auto duration = section == 3 ? ComboRecoveryDuration : attackDuration;
        const auto start = section == 3 ? recoveryStart : attackDuration * float(section);
        const auto frames = int(std::ceil(duration * 60));
        for (int frame = 0; frame <= frames; ++frame)
        {
            const auto time = duration * float(frame) / float(frames);
            const auto timelineTime = start + time;
            SLocalPose pose;
            const auto sampleTime = section == 3 ? attackDuration : time;
            if (!attack->Sample(sampleTime, pose, reference))
                return nullptr;
            for (size_t bone = 0; bone < tracks.size(); ++bone)
            {
                auto& track = tracks[bone];
                // A shared boundary uses the following section's first sample.
                if (!track.Translations.empty() &&
                    timelineTime <= track.Translations.back().Time)
                {
                    track.Translations.pop_back();
                    track.Rotations.pop_back();
                    track.Scales.pop_back();
                }
                track.Translations.push_back({timelineTime, pose.LocalTransforms[bone].Translation});
                track.Rotations.push_back({timelineTime, pose.LocalTransforms[bone].Rotation});
                track.Scales.push_back({timelineTime, pose.LocalTransforms[bone].Scale});
            }
        }
    }
    std::vector<SAnimationNotify> notifies;
    for (int i = 0; i < 3; ++i)
        notifies.push_back({"PunchHit", attackDuration * (float(i) + 0.45f)});
    for (int i = 0; i < 3; ++i)
        notifies.push_back({"AttackHitbox", attackDuration * (float(i) + 0.3f),
            attackDuration * 0.35f, SAttackWindowPayload{}});
    std::vector<SAnimationCurveKey> gripKeys;
    for (int i = 0; i < 3; ++i)
    {
        const auto start = attackDuration * float(i);
        for (const auto& [phase, value] : std::array{
            std::pair{0.0f, 1.0f}, std::pair{0.15f, 1.0f}, std::pair{0.3f, 0.0f},
            std::pair{0.65f, 0.0f}, std::pair{0.85f, 1.0f}})
            gripKeys.push_back({start + attackDuration * phase, value});
    }
    gripKeys.push_back({recoveryStart, 1});
    return Sequence::Create(
        "PunchCombo",
        attack->GetSkeleton(),
        recoveryStart + ComboRecoveryDuration,
        std::move(tracks),
        {},
        std::move(notifies),
        {{"SupportHandIK", std::move(gripKeys)}}
    );
}

Ref<const Sequence> Dissolve::CreateRecoilAnimation()
{
    const auto& skeleton = m_MeshInstance->GetMesh()->GetSkeleton();
    const auto chest = skeleton->FindBone("chest");
    if (chest < 0)
    {
        EE_CORE_ERROR("The recoil demo requires a chest bone.")
        return nullptr;
    }

    const auto& reference = m_MeshInstance->GetMesh()->GetData().ReferencePose.LocalTransforms[size_t(chest)];
    SBoneAnimationTrack track;
    track.BoneIndex = uint32_t(chest);
    track.Translations = {
        { 0.0f, reference.Translation },
        { 0.08f, reference.Translation + glm::vec3(0, 0.03f, -0.04f) },
        { 0.22f, reference.Translation },
        { 0.65f, reference.Translation },
    };
    track.Rotations = {
        { 0.0f, reference.Rotation },
        { 0.08f, glm::angleAxis(glm::radians(-18.0f), glm::vec3(1, 0, 0)) * reference.Rotation },
        { 0.22f, glm::angleAxis(glm::radians(3.0f), glm::vec3(1, 0, 0)) * reference.Rotation },
        { 0.65f, reference.Rotation },
    };
    return Sequence::Create(
        "RecoilPulse",
        skeleton,
        0.65f,
        { std::move(track) },
        {},
        {
            { "RecoilPeak", 0.08f }
        }
    );
}

void Dissolve::UpdateAnimation(const float deltaSeconds)
{
    if (!m_AnimationGraph)
        return;

    const auto requested = m_RequestedForward.load(std::memory_order_relaxed);
    const auto current = m_AnimationForward.load(std::memory_order_relaxed);
    constexpr float acceleration = 3.0f;
    const auto step = acceleration * deltaSeconds;
    const auto forward = current + std::clamp(requested - current, -step, step);
    const auto strafe = m_AnimationStrafe.load(std::memory_order_relaxed);
    const auto targetStrafe = m_RequestedStrafe.load(std::memory_order_relaxed);
    const auto nextStrafe = strafe + std::clamp(targetStrafe - strafe, -step, step);
    const auto paused = m_AnimationPaused.load(std::memory_order_relaxed);
    const auto advance = m_StepAnimation.exchange(false, std::memory_order_relaxed)
        ? 1.0f / 30.0f : (paused ? 0.0f : deltaSeconds);
    auto* upperBody = m_Animation->GetLayer("UpperBody");
    auto* comparisonAction = m_ComparisonAnimation->GetLayer("UpperBody");
    auto* recoil = m_Animation->GetLayer("Recoil");
    if (!upperBody || !comparisonAction || !recoil)
        return;
    const auto action = m_ActionRequest.exchange(0, std::memory_order_relaxed);
    if (action == 1 && upperBody->GetPlayer().GetMontage())
    {
        if (!upperBody->GetPlayer().JumpToMontageSection("Recovery") ||
            !comparisonAction->GetPlayer().JumpToMontageSection("Recovery"))
            return;
    }
    else if (action == 2)
    {
        if (!upperBody->GetPlayer().GetMontage())
        {
            if (!upperBody->GetPlayer().PlayMontage(m_PunchMontage) ||
                !comparisonAction->GetPlayer().PlayMontage(m_PunchMontage))
                return;
        }
        else
        {
            const auto section = upperBody->GetPlayer().GetCurrentMontageSection();
            const auto next = section == "Attack1" ? "Attack2"
                : section == "Attack2" ? "Attack3" : "";
            if (*next && (!upperBody->GetPlayer().SetNextMontageSection(section, next) ||
                !comparisonAction->GetPlayer().SetNextMontageSection(section, next)))
                return;
        }
    }
    const auto dodge = m_DodgeRequested.exchange(false, std::memory_order_relaxed);
    auto& locomotion = m_Animation->GetLayer("Locomotion")->GetPlayer();
    auto& comparisonLocomotion = m_ComparisonAnimation->GetLayer("Locomotion")->GetPlayer();
    if (dodge && (!locomotion.PlayMontage(m_DodgeMontage) ||
        !comparisonLocomotion.PlayMontage(m_DodgeMontage)))
        return;
    const auto dodgeActive = bool(locomotion.GetMontage());
    const auto layerWeight = m_LayerWeight.load(std::memory_order_relaxed);
    const auto targetWeight = !dodgeActive && m_LayerEnabled.load(std::memory_order_relaxed)
        ? 1.0f : 0.0f;
    const auto nextWeight = layerWeight +
        std::clamp(targetWeight - layerWeight, -advance * 4.0f, advance * 4.0f);
    const auto recoilWeight = m_RecoilEnabled.load(std::memory_order_relaxed)
        ? m_RecoilStrength.load(std::memory_order_relaxed) : 0.0f;
    if (!upperBody->SetWeight(nextWeight) || !comparisonAction->SetWeight(nextWeight) ||
        !recoil->SetWeight(recoilWeight))
        return;
    m_LayerWeight.store(nextWeight, std::memory_order_relaxed);

    if (m_IKModifier)
    {
        const auto ikWeight = m_IKEnabled.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        const std::array constraints = {
            STwoBoneIKConstraint{
                .EndBoneIndex = m_IKDriverBone,
                .TargetPosition = m_IKTarget,
                .PolePosition = {-0.45f, 0.55f, 0.25f},
                .Weight = ikWeight,
                .TargetRotation = m_IKDriverRotation
            },
            STwoBoneIKConstraint{
                .EndBoneIndex = m_IKEndBone,
                .TargetPosition = m_IKSupportOffset.Translation,
                .PolePosition = {0.45f, 0.55f, 0.25f},
                .Weight = ikWeight,
                .TargetRotation = m_IKRotationEnabled.load(std::memory_order_relaxed)
                    ? std::optional(m_IKSupportOffset.Rotation) : std::nullopt,
                .TargetReference = std::string("SupportGrip"),
                .WeightCurve = "SupportHandIK"
            }
        };
        if (!m_IKModifier->SetConstraints(constraints)) return;
    }

    if (!m_Animation->GetParameters().SetFloat("Forward", forward) ||
        !m_Animation->GetParameters().SetFloat("Strafe", nextStrafe) ||
        !m_ComparisonAnimation->GetParameters().SetFloat("Forward", forward) ||
        !m_ComparisonAnimation->GetParameters().SetFloat("Strafe", nextStrafe) ||
        !m_Animation->Update(advance) ||
        !m_ComparisonAnimation->Update(advance))
        return;

    m_SupportHandIKWeight.store(
        m_Animation->GetCurveValues().GetValue("SupportHandIK"),
        std::memory_order_relaxed
    );
    m_CharacterMotion *= m_Animation->GetRootMotionDelta().ToMatrix();
    if (m_ResetMotion.exchange(false, std::memory_order_relaxed))
        m_CharacterMotion = glm::mat4(1);
    m_RootMotionDistance.store(glm::length(glm::vec3(m_CharacterMotion[3])),
        std::memory_order_relaxed);

    const auto section = upperBody->GetPlayer().GetCurrentMontageSection();
    const auto sectionIndex = section == "Attack1" ? 1 : section == "Attack2" ? 2
        : section == "Attack3" ? 3 : section == "Recovery" ? 4 : 0;
    m_ComboSection.store(sectionIndex, std::memory_order_relaxed);

    if (m_SeekContact.exchange(false, std::memory_order_relaxed))
    {
        const auto duration = m_LocomotionSpace->GetDuration(std::array{nextStrafe, forward});
        if (!m_Animation->Seek(m_ContactPhase * duration) ||
            !m_ComparisonAnimation->Seek(m_ContactPhase * duration))
            return;
    }

    for (const auto& event : m_Animation->GetNotifyEvents())
    {
        if (event.LayerName != "UpperBody" ||
            event.GetName() != "AttackHitbox")
            continue;
        const auto* payload = event.GetPayload<SAttackWindowPayload>();
        if (!payload)
        {
            EE_CORE_ERROR("Attack window requires an SAttackWindowPayload.")
            continue;
        }
        switch (event.Type)
        {
            case EAnimationNotifyEventType::Instant:
                break;
            case EAnimationNotifyEventType::Begin:
                m_HitboxActive.store(true, std::memory_order_relaxed);
                break;
            case EAnimationNotifyEventType::Update:
                // A game would query targets during this active interval.
                m_NotifyUpdates.fetch_add(1, std::memory_order_relaxed);
                break;
            case EAnimationNotifyEventType::End:
                m_HitboxActive.store(false, std::memory_order_relaxed);
                if (event.Cancelled)
                    m_CancelledNotifies.fetch_add(1, std::memory_order_relaxed);
                break;
        }
    }

    // The application maps generic animation events to feedback for this demo.
    for (const auto& event : m_Animation->GetNotifyEvents())
    {
        if (event.Type != EAnimationNotifyEventType::Instant)
            continue;

        if (event.LayerName == "UpperBody")
            m_ActionEvents.fetch_add(1, std::memory_order_relaxed);
        if (event.LayerName == "Recoil")
            m_RecoilEvents.fetch_add(1, std::memory_order_relaxed);
        if (event.GetName() == "Footstep")
        {
            const auto* payload = event.GetPayload<SFootstepNotifyPayload>();
            if (!payload)
            {
                EE_CORE_ERROR("Footstep notify requires an SFootstepNotifyPayload.")
                continue;
            }

            m_SynchronizedSteps.fetch_add(1, std::memory_order_relaxed);
            m_LastFoot.store(int(payload->Foot), std::memory_order_relaxed);
        }
    }
    for (const auto& event : m_ComparisonAnimation->GetNotifyEvents())
        if (event.GetName() == "Footstep")
            m_ComparisonSteps.fetch_add(1, std::memory_order_relaxed);

    m_AnimationPhase.store(m_Animation->GetNormalizedTime(), std::memory_order_relaxed);
    m_AnimationForward.store(forward, std::memory_order_relaxed);
    m_AnimationStrafe.store(nextStrafe, std::memory_order_relaxed);
}

void Dissolve::InitializeSocketCharacter()
{
    if (auto loaded = MeshLoaderRegistry::LoadSkeletalMesh(
        "./Assets/Meshes/KayKitCharacter/Locomotion.glb"
    ))
    {
        auto data = (*loaded)->GetData();
        const auto hand = data.Skeleton->FindBone("handslot.r");
        const auto wrist = data.Skeleton->FindBone("wrist.l");
        if (hand < 0 || wrist < 0)
        {
            EE_CORE_ERROR("Socket demonstration requires the right hand and left wrist.")
            return;
        }

        data.Skeleton = data.Skeleton->WithSockets({{
            .Name = "WeaponGrip", .BoneIndex = uint32_t(hand),
            .RelativeTransform = {
                .Rotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 0, 1))
            }
        }, {
            .Name = "SupportGrip", .BoneIndex = uint32_t(hand),
            .RelativeTransform = {
                .Translation = {0, -0.35f, 0},
                .Rotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 0, 1))
            }
        }});
        if (!data.Skeleton) return;
        mesh = SkeletalMesh::Create(std::move(data));
        if (!mesh) return;

        const auto& bounds = mesh->GetLocalBounds();
        const auto separation = (bounds.Max.y - bounds.Min.y) * 0.7f;
        m_CameraController->FrameBounds(
            bounds.Min - glm::vec3(separation, 0, 0),
            bounds.Max + glm::vec3(separation, 0, 0)
        );
        m_CameraController->SetDistance((bounds.Max.y - bounds.Min.y) * 2.0f);

        m_MeshInstance = SkeletalMeshInstance::Create(mesh);
        m_ComparisonInstance = SkeletalMeshInstance::Create(mesh);
        InitializeAnimationGraph();
        InitializeSocketAttachment();
    }
}

void Dissolve::InitializeSocketAttachment()
{
    if (!InitializeAttachmentIK()) return;
    InitializeBaton();
}

bool Dissolve::InitializeAttachmentIK()
{
    const auto wrist = mesh->GetSkeleton()->FindBone("wrist.l");
    const auto driver = mesh->GetSkeleton()->FindBone("wrist.r");
    const auto leftGrip = mesh->GetSkeleton()->FindBone("handslot.l");
    if (wrist < 0 || driver < 0 || leftGrip < 0)
    {
        EE_CORE_ERROR("Two-bone IK demonstration requires both wrists and the left grip.")
        return false;
    }
    m_IKEndBone = uint32_t(wrist);
    m_IKDriverBone = uint32_t(driver);
    SModelPose reference;
    if (!mesh->GetSkeleton()->BuildModelPose(mesh->GetData().ReferencePose, reference)) return false;
    const auto wristFromGrip = glm::inverse(reference.Matrices[size_t(leftGrip)]) *
        reference.Matrices[size_t(wrist)];
    m_IKSupportOffset.Translation = glm::vec3(wristFromGrip[3]);
    m_IKSupportOffset.Rotation = glm::normalize(glm::quat_cast(glm::mat3(wristFromGrip)));
    const auto* socket = mesh->GetSkeleton()->GetSocket("WeaponGrip");
    if (!socket) return false;
    const auto weaponFrame = socket->GetTransform(reference);
    if (!weaponFrame) return false;
    const auto wristRotation = glm::normalize(
        glm::quat_cast(glm::mat3(reference.Matrices[size_t(driver)]))
    );
    const auto weaponRotation = glm::normalize(glm::quat_cast(glm::mat3(*weaponFrame)));
    const auto relativeRotation = glm::conjugate(wristRotation) * weaponRotation;
    m_IKDriverRotation = glm::normalize(
        glm::angleAxis(glm::radians(-60.0f), glm::vec3(1, 0, 0)) *
        glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1)) *
        glm::conjugate(relativeRotation)
    );
    if (m_Animation)
    {
        const auto modifier = TwoBoneIK::Create(mesh->GetSkeleton());
        if (!modifier) return false;
        const std::array<Ref<const PoseModifier>, 1> modifiers{modifier};
        if (!m_Animation->SetPoseModifiers(modifiers)) return false;
        m_IKModifier = modifier;
    }
    return true;
}

void Dissolve::InitializeBaton()
{
    SStaticMeshData data;
    data.Name = "SocketBaton";
    const std::array<glm::vec3, 8> corners = {{
        {-0.035f, 0, -0.035f}, {0.035f, 0, -0.035f},
        {0.035f, 0.5f, -0.035f}, {-0.035f, 0.5f, -0.035f},
        {-0.035f, 0, 0.035f}, {0.035f, 0, 0.035f},
        {0.035f, 0.5f, 0.035f}, {-0.035f, 0.5f, 0.035f}
    }};
    const std::array<uint32_t, 36> triangles = {
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
        0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2
    };
    for (size_t i = 0; i < triangles.size(); i += 3)
    {
        const auto tangent = glm::normalize(corners[triangles[i + 1]] - corners[triangles[i]]);
        const auto edge = corners[triangles[i + 2]] - corners[triangles[i]];
        const auto normal = glm::normalize(glm::cross(tangent, edge));
        for (size_t j = 0; j < 3; ++j)
        {
            data.Indices.push_back(uint32_t(data.Vertices.size()));
            data.Vertices.push_back({ .Position = corners[triangles[i + j]],
                .Normal = normal, .Tangent = glm::vec4(tangent, 1) });
        }
    }
    data.LocalBounds = {.Min = {-0.035f, 0, -0.035f}, .Max = {0.035f, 0.5f, 0.035f}};
    data.Sections = {{.IndexCount = uint32_t(data.Indices.size()), .LocalBounds = data.LocalBounds}};
    MaterialGraph graph;
    const auto color = graph.AddNode<Constant>(
        glm::vec4(1, 0.2f, 0.025f, 1),
        EMaterialValueType::Float4
    );
    graph.SetChannel(EMaterialChannel::BaseColor, color);
    auto material = CreateRef<Material>("SocketBaton", EMaterialUsage::Surface);
    material->SetGraph(std::move(graph));
    if (!GetMaterialRegistry().Register(material)) return;
    data.Materials = {std::move(material)};
    m_AttachmentGeometryPool = CreateScope<GeometryPool>(
        *m_GraphicsContext,
        SGeometryPoolConfig{.VertexCapacity = 36, .IndexCapacity = 36}
    );
    auto geometry = m_AttachmentGeometryPool->Upload(data);
    if (!geometry.IsValid()) return;
    m_AttachmentMesh = StaticMesh::Create(
        data.Name,
        std::move(geometry),
        std::move(data.Sections),
        std::move(data.Materials),
        data.LocalBounds
    );
    if (!m_AttachmentMesh) return;
    m_AttachmentRenderer = CreateScope<StaticMeshRenderer>(
        *m_GraphicsContext,
        GetMaterialSystem(),
        *m_AttachmentGeometryPool
    );
}

void Dissolve::DrawGeometry()
{
    if (m_SkeletalMeshRenderer && m_MeshInstance)
    {
        // Show the calibrated grip from the front for direct pose comparison.
        const auto& bounds = m_MeshInstance->GetMesh()->GetLocalBounds();
        const auto center = (bounds.Min + bounds.Max) * 0.5f;
        const auto separation = (bounds.Max.y - bounds.Min.y) * 0.7f;
        const auto& cameraPosition = m_CameraController->GetCamera().GetPosition();
        const auto centerDirection = cameraPosition - center;
        const auto centerYaw = std::atan2(centerDirection.x, centerDirection.z);
        const auto renderInstance = [&](const SkeletalMeshInstance& instance,
            const Animation* animation, const float offset)
        {
            const auto position = center + glm::vec3(offset, 0, 0);
            const auto direction = cameraPosition - position;
            // Compensate perspective bearing so both copies show the same viewing angle.
            const auto correction = std::atan2(direction.x, direction.z) - centerYaw;
            const auto transform = glm::translate(glm::mat4(1), position) *
                glm::rotate(glm::mat4(1), correction, glm::vec3(0, 1, 0)) *
                glm::translate(glm::mat4(1), -center);
            const auto movement = &instance == m_MeshInstance.get()
                ? m_CharacterMotion : glm::mat4(1);
            if (animation)
                m_SkeletalMeshRenderer->Render(instance, animation->GetModelPose(), transform * movement);
            else
                m_SkeletalMeshRenderer->Render(instance, transform * movement);
            if (m_AttachmentRenderer)
            {
                const auto& pose = animation ? animation->GetModelPose()
                    : instance.GetReferenceModelPose();
                const auto& skeleton = instance.GetMesh()->GetSkeleton();
                const auto* socket = skeleton->GetSocket("WeaponGrip");
                if (socket)
                {
                    const auto attachment = socket->GetTransform(pose);
                    if (attachment)
                    {
                        m_AttachmentRenderer->Render(
                            m_AttachmentMesh,
                            transform * movement * *attachment
                        );
                    }
                }
            }
        };
        renderInstance(*m_ComparisonInstance, m_ComparisonAnimation.get(), separation);
        renderInstance(*m_MeshInstance, m_Animation.get(), -separation);
    }
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
