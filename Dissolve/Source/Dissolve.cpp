#include "Dissolve.h"

#include "Engine/Materials/Nodes/Color.h"
#include "Engine/Materials/Nodes/Parameter.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Aether/Manager.h>
#include <Engine/GUI/Canvas.h>

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
    /** Identifies the foot associated with a locomotion event. */
    enum class EFoot : uint8_t { Left = 1, Right = 2 };

    /** Carries application-specific footstep data without engine-owned gameplay types. */
    struct SFootstepNotifyPayload
    {
        EFoot Foot;
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

        if (auto loaded = MeshLoaderRegistry::LoadSkeletalMesh(
                "./Assets/Meshes/KayKitCharacter/Locomotion.glb"
            ))
        {
            mesh = std::move(*loaded);

            const auto& bounds = mesh->GetLocalBounds();
            const auto separation = (bounds.Max.y - bounds.Min.y) * 0.7f;
            m_CameraController->FrameBounds(
                bounds.Min - glm::vec3(separation, 0, 0),
                bounds.Max + glm::vec3(separation, 0, 0)
            );
            m_CameraController->SetDistance((bounds.Max.y - bounds.Min.y) * 2.0f);

            m_MeshInstance = CreateScope<SkeletalMeshInstance>(mesh);
            m_ComparisonInstance = CreateScope<SkeletalMeshInstance>(mesh);
            InitializeAnimationController();
        }
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
        "WASD/Arrows: direction | 1: walk/run | 2: forward/strafe | 3: back/strafe"
    );
    instructions->SetFontSize(20.0f);
    panel->AddChild(instructions)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 20})
        .SetSize({1100, 30});
    const auto playbackInstructions = CreateRef<GUI::TextBlock>(
        "P: pause | N: next frame | F: left-foot contact | Space: idle | Shift/R: run"
    );
    playbackInstructions->SetFontSize(18.0f);
    panel->AddChild(playbackInstructions)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 50})
        .SetSize({1000, 30});
    for (const auto& [position, caption] : std::array{
        std::pair{0.25f, "NORMALIZED TIME"}, std::pair{0.75f, "SYNC MARKERS"}
    })
    {
        const auto label = CreateRef<GUI::TextBlock>(caption);
        label->SetFontSize(24.0f);
        panel->AddChild(label)
            .SetAnchors({position, 0, position, 0})
            .SetAlignment({0.5f, 0})
            .SetPosition({0, 130})
            .SetSize({260, 35});
    }
    m_AnimationStatus = CreateRef<GUI::TextBlock>("KayKit sync comparison");
    m_AnimationStatus->SetFontSize(20.0f);
    panel->AddChild(m_AnimationStatus)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 85})
        .SetSize({1100, 30});
    m_GUIManager->SetRoot(panel);

    m_GraphicsContext->SetClearColor({0.015f, 0.025f, 0.06f, 1.0f});

    // Start the demo clock after loading assets so the first animation remains visible.
    m_Timer = Timer{};
}

Dissolve::~Dissolve()
{
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
                "X: {:.2f} | Y: {:.2f} | Phase: {:.3f} | {} | Steps: {} / {} | Last: {}",
                m_AnimationStrafe.load(std::memory_order_relaxed),
                m_AnimationForward.load(std::memory_order_relaxed),
                m_AnimationPhase.load(std::memory_order_relaxed),
                m_AnimationPaused.load(std::memory_order_relaxed) ? "PAUSED" : "PLAYING",
                m_ComparisonSteps.load(std::memory_order_relaxed),
                m_SynchronizedSteps.load(std::memory_order_relaxed),
                m_LastFoot.load(std::memory_order_relaxed) == 1 ? "Left" :
                    (m_LastFoot.load(std::memory_order_relaxed) == 2 ? "Right" : "None")
            )
        );
}

void Dissolve::Prepare(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Prepare(frameTime);

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
    DrawGeometry();
    m_SkeletalMeshRenderer->EndFrame();

    // Keep particle effects out of the locomotion comparison.
}

void Dissolve::OnEvent(Event& event)
{
    Application::OnEvent(event);
    m_CameraController->ProcessEvent(event);
}

void Dissolve::InitializeAnimationController()
{
    const auto& animations = m_MeshInstance->GetMesh()->GetAnimations();
    const auto findAnimation = [&](const std::string_view name) -> Ref<const AnimationSequence>
    {
        const auto found = std::ranges::find_if(
            animations,
            [&](const auto& animation)
            {
                return animation->GetName() == name;
            }
        );
        return found == animations.end() ? nullptr : *found;
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
        const Ref<const AnimationSequence>& clip,
        const float leftTime,
        const float rightTime
    )
    {
        return clip->WithNotifies({
            { "Footstep", leftTime, SFootstepNotifyPayload{ EFoot::Left } },
            { "Footstep", rightTime, SFootstepNotifyPayload{ EFoot::Right } },
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
    const auto comparisonSpace = BlendSpace2D::Create(
        "KayKitNormalized",
        {
            { { 0, 0 }, idle },
            { { 0, 1.5f }, walk },
            { { 0, 3 }, run },
            { { 0, -1.5f }, backward },
            { { -3, 0 }, left },
            { { 3, 0 }, right },
        }
    );
    if (!blendSpace || !comparisonSpace)
        return;

    const auto makeController = [](Ref<const BlendSpace> space)
    {
        return AnimationController::Create({
            .Name = "KayKitLocomotion",
            .InitialState = "Locomotion",
            .Parameters = {
                { "Strafe", 0.0f },
                { "Forward", 0.0f }
            },
            .States = {{
                .Name = "Locomotion",
                .BlendSpace = std::move(space),
                .BlendParameters = { "Strafe", "Forward" },
            }},
        });
    };
    const auto controller = makeController(blendSpace);
    const auto comparisonController = makeController(comparisonSpace);
    if (!controller || !comparisonController)
        return;

    auto instance = CreateScope<AnimationControllerInstance>(
        controller,
        *m_MeshInstance
    );
    auto comparison = CreateScope<AnimationControllerInstance>(
        comparisonController,
        *m_ComparisonInstance
    );
    if (!instance->Update(0.0f) || !comparison->Update(0.0f))
        return;

    m_LocomotionSpace = blendSpace;
    m_ContactPhase = syncLeft->GetSyncMarkers().front().Time / syncLeft->GetDuration();
    m_AnimationController = std::move(instance);
    m_ComparisonController = std::move(comparison);
    EE_CORE_INFO("KayKit comparison: left normalized, right markers; P pauses, N steps, F contact.")
}

void Dissolve::UpdateAnimation(const float deltaSeconds)
{
    if (!m_AnimationController || !m_ComparisonController)
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
    if (!m_AnimationController->SetFloat("Forward", forward) ||
        !m_AnimationController->SetFloat("Strafe", nextStrafe) ||
        !m_ComparisonController->SetFloat("Forward", forward) ||
        !m_ComparisonController->SetFloat("Strafe", nextStrafe) ||
        !m_AnimationController->Update(advance) ||
        !m_ComparisonController->Update(advance))
        return;

    if (m_SeekContact.exchange(false, std::memory_order_relaxed))
    {
        const auto duration = m_LocomotionSpace->GetDuration(std::array{nextStrafe, forward});
        if (!m_MeshInstance->Seek(m_ContactPhase * duration) ||
            !m_ComparisonInstance->Seek(m_ContactPhase * duration))
            return;
    }

    // The application maps generic animation events to feedback for this demo.
    for (const auto& event : m_MeshInstance->GetNotifyEvents())
    {
        if (event.GetNotify().Name == "Footstep")
        {
            const auto* payload = event.GetNotify().GetPayload<SFootstepNotifyPayload>();
            if (!payload)
            {
                EE_CORE_ERROR("Footstep notify requires an SFootstepNotifyPayload.")
                continue;
            }

            m_SynchronizedSteps.fetch_add(1, std::memory_order_relaxed);
            m_LastFoot.store(int(payload->Foot), std::memory_order_relaxed);
        }
    }
    for (const auto& event : m_ComparisonInstance->GetNotifyEvents())
        if (event.GetNotify().Name == "Footstep")
            m_ComparisonSteps.fetch_add(1, std::memory_order_relaxed);

    m_AnimationPhase.store(m_MeshInstance->GetNormalizedTime(), std::memory_order_relaxed);
    m_AnimationForward.store(forward, std::memory_order_relaxed);
    m_AnimationStrafe.store(nextStrafe, std::memory_order_relaxed);
}

void Dissolve::DrawGeometry()
{
    if (m_SkeletalMeshRenderer && m_MeshInstance)
    {
        // Show the mannequin from an angle that keeps all limbs visible.
        const auto& bounds = m_MeshInstance->GetMesh()->GetLocalBounds();
        const auto center = (bounds.Min + bounds.Max) * 0.5f;
        const auto separation = (bounds.Max.y - bounds.Min.y) * 0.7f;
        const auto& cameraPosition = m_CameraController->GetCamera().GetPosition();
        const auto centerDirection = cameraPosition - center;
        const auto centerYaw = std::atan2(centerDirection.x, centerDirection.z);
        const auto renderInstance = [&](const SkeletalMeshInstance& instance, const float offset)
        {
            const auto position = center + glm::vec3(offset, 0, 0);
            const auto direction = cameraPosition - position;
            // Compensate perspective bearing so both copies show the same viewing angle.
            const auto correction = std::atan2(direction.x, direction.z) - centerYaw;
            const auto transform = glm::translate(glm::mat4(1), position) *
                glm::rotate(glm::mat4(1), glm::radians(25.0f) + correction,
                    glm::vec3(0, 1, 0)) *
                glm::translate(glm::mat4(1), -center);
            m_SkeletalMeshRenderer->Render(instance, transform);
        };
        renderInstance(*m_ComparisonInstance, separation);
        renderInstance(*m_MeshInstance, -separation);
    }
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
