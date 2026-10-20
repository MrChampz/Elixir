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
            m_CameraController->FrameBounds(bounds.Min, bounds.Max);

            m_MeshInstance.SetMesh(mesh);
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
        "WASD: move | Shift/R: run | Arrows: blend X/Y | Space: idle"
    );
    instructions->SetFontSize(20.0f);
    panel->AddChild(instructions)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 20})
        .SetSize({650, 30});
    m_AnimationStatus = CreateRef<GUI::TextBlock>("BlendSpace2D | X: 0.00 | Y: 0.00");
    m_AnimationStatus->SetFontSize(20.0f);
    panel->AddChild(m_AnimationStatus)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({20, 55})
        .SetSize({650, 30});
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
                "BlendSpace2D | X: {:.2f} | Y: {:.2f}",
                m_AnimationStrafe.load(std::memory_order_relaxed),
                m_AnimationForward.load(std::memory_order_relaxed)
            )
        );
}

void Dissolve::Prepare(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Prepare(frameTime);

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

    aether.Render(m_CameraController->GetCamera(), m_SceneTarget);
}

void Dissolve::OnEvent(Event& event)
{
    Application::OnEvent(event);
    m_CameraController->ProcessEvent(event);
}

void Dissolve::InitializeAnimationController()
{
    const auto& animations = m_MeshInstance.GetMesh()->GetAnimations();
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
    const auto walk = findAnimation("Walking_A");
    const auto run = findAnimation("Running_A");
    const auto backward = findAnimation("Walking_Backwards");
    const auto left = findAnimation("Running_Strafe_Left");
    const auto right = findAnimation("Running_Strafe_Right");
    if (!idle || !walk || !run || !backward || !left || !right)
    {
        EE_CORE_ERROR("KayKit locomotion requires all six directional animations.")
        return;
    }
    const auto blendSpace = BlendSpace2D::Create(
        "KayKitLocomotion", {
                                {{0, 0}, idle},
                                {{0, 1.5f}, walk},
                                {{0, 3}, run},
                                {{0, -1.5f}, backward},
                                {{-3, 0}, left},
                                {{3, 0}, right},
                            }
    );
    if (!blendSpace)
        return;

    const auto controller = AnimationController::Create({
        .Name = "KayKitLocomotion",
        .InitialState = "Locomotion",
        .Parameters = {
            { "Strafe", 0.0f },
            { "Forward", 0.0f }
        },
        .States = {{
            .Name = "Locomotion",
            .BlendSpace = blendSpace,
            .BlendParameters = { "Strafe", "Forward" },
        }},
    });
    if (!controller)
        return;

    auto instance = CreateScope<AnimationControllerInstance>(controller, m_MeshInstance);
    if (!instance->Update(0.0f))
        return;

    m_AnimationController = std::move(instance);
    EE_CORE_INFO("KayKit 2D: WASD to move, Shift/R to run, arrows set X/Y, Space resets.")
}

void Dissolve::UpdateAnimation(const float deltaSeconds)
{
    if (!m_AnimationController)
        return;

    const auto requested = m_RequestedForward.load(std::memory_order_relaxed);
    const auto current = m_AnimationForward.load(std::memory_order_relaxed);
    constexpr float acceleration = 3.0f;
    const auto step = acceleration * deltaSeconds;
    const auto forward = current + std::clamp(requested - current, -step, step);
    const auto strafe = m_AnimationStrafe.load(std::memory_order_relaxed);
    const auto targetStrafe = m_RequestedStrafe.load(std::memory_order_relaxed);
    const auto nextStrafe = strafe + std::clamp(targetStrafe - strafe, -step, step);
    if (!m_AnimationController->SetFloat("Forward", forward) ||
        !m_AnimationController->SetFloat("Strafe", nextStrafe) ||
        !m_AnimationController->Update(deltaSeconds))
        return;

    m_AnimationForward.store(forward, std::memory_order_relaxed);
    m_AnimationStrafe.store(nextStrafe, std::memory_order_relaxed);
}

void Dissolve::DrawGeometry()
{
    if (m_SkeletalMeshRenderer && m_MeshInstance.GetMesh())
    {
        // Show the mannequin from an angle that keeps all limbs visible.
        const auto& bounds = m_MeshInstance.GetMesh()->GetLocalBounds();
        const auto center = (bounds.Min + bounds.Max) * 0.5f;
        const auto transform =
            glm::translate(glm::mat4(1), center) *
            glm::rotate(glm::mat4(1), glm::radians(25.0f), glm::vec3(0, 1, 0)) *
            glm::translate(glm::mat4(1), -center);
        m_SkeletalMeshRenderer->Render(m_MeshInstance, transform);
    }
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
