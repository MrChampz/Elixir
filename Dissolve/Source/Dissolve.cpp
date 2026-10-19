#include "Dissolve.h"

#include "Engine/Materials/Nodes/Color.h"
#include "Engine/Materials/Nodes/Parameter.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Aether/Manager.h>

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
        .DirectionalLightDirection = glm::normalize(
            glm::vec3{ -0.5f, 0.65f, -0.55f }
        ),
        .DirectionalLightColor = { 1.0f, 0.96f, 0.9f },
        .DirectionalLightIntensity = 2.2f,
        .DebugView = ESurfaceDebugView::Composite,
    };

    m_ParticleSystems[0] = GetAetherManager().LoadEffect("./Assets/VFX/FireAndFireworks.json");
    EE_CORE_ASSERT(
        m_ParticleSystems[0],
        "Could not resolve FireAndFireworks effect."
    )

    m_ParticleSystems[1] = GetAetherManager().LoadEffect("./Assets/VFX/RibbonVortex.json");
    EE_CORE_ASSERT(
        m_ParticleSystems[1],
        "Could not resolve RibbonVortex effect."
    )

    {
        MaterialGraph graph;

        graphMaterial = CreateRef<Material>("DissolveGraph", EMaterialUsage::Particle);
        EE_CORE_ASSERT(
            graphMaterial->SetBlendMode(EMaterialBlendMode::Translucent),
            "Dissolve graph material must enable translucent blending."
        )

        EE_CORE_ASSERT(graphMaterial->DefineParameter("Tint", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialValueType::Float4,
            .DefaultValue = SMaterialParameter::MakeVector({ 1.0f, 0.5f, 0.2f, 1.0f }),
        }), "")

        EE_CORE_ASSERT(graphMaterial->DefineParameter("Albedo", {
            .Kind = EMaterialParameterKind::Texture,
            .DefaultValue = SMaterialParameter::MakeTexture(tex),
        }), "")

        const auto albedo = graph.AddNode<TextureSample>("Albedo");
        const auto tint = graph.AddNode<Parameter>("Tint", EMaterialValueType::Float4);

        const auto baseColor = graph.AddNode<Multiply>();
        graph.Connect(albedo, baseColor, 0);
        graph.Connect(tint, baseColor, 0);

        graph.SetChannel(EMaterialChannel::BaseColor, baseColor);

        graphMaterial->SetGraph(std::move(graph));
        EE_CORE_ASSERT(GetMaterialRegistry().Register(graphMaterial), "GraphMaterial must be unique.")

        auto instance = graphMaterial->CreateInstance();
        EE_CORE_ASSERT(
            instance->SetVector("Tint", { 1.0f, 0.35f, 0.1f, 1.0f }),
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

        if (auto loaded = MeshLoaderRegistry::LoadSkeletalMesh("./Assets/Meshes/Fox/Fox.glb"))
        {
            mesh = std::move(*loaded);

            const auto& bounds = mesh->GetLocalBounds();
            m_CameraController->FrameBounds(bounds.Min, bounds.Max);

            m_MeshInstance.SetMesh(mesh);
            if (!mesh->GetAnimations().empty())
            {
                const auto& animation = mesh->GetAnimations().front();
                if (m_MeshInstance.Play(animation))
                    EE_CORE_INFO("Animation demo: playing '{}'.", animation->GetName())
            }
        }
    }

    {
        MaterialGraph graph1;

        const auto ribbonMaterial = CreateRef<Material>("RibbonEnergy", EMaterialUsage::Particle);
        EE_CORE_ASSERT(
            ribbonMaterial->SetBlendMode(EMaterialBlendMode::Translucent),
            "Ribbon material must enable translucent blending."
        )

        EE_CORE_ASSERT(ribbonMaterial->DefineParameter("Tint", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialValueType::Float4,
            .DefaultValue = SMaterialParameter::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
        }), "")

        const auto tint = graph1.AddNode<Parameter>(
            "Tint",
            EMaterialValueType::Float4
        );

        const auto color = graph1.AddNode<Nodes::Color>();

        const auto multiply = graph1.AddNode<Multiply>();
        graph1.Connect(tint, multiply, 0);
        graph1.Connect(color, multiply, 1);
        graph1.SetChannel(EMaterialChannel::BaseColor, multiply);

        // const auto glow = graph1.AddNode<Parameter>("Glow", EMaterialValueType::Float4);
        //graph1.SetChannel(EMaterialChannel::Emissive, glow);

        ribbonMaterial->SetGraph(std::move(graph1));
        EE_CORE_ASSERT(GetMaterialRegistry().Register(ribbonMaterial), "RibbonEnergy must be unique.")

        if (auto* emitter = m_ParticleSystems[1]->FindEmitter("PathRibbon"))
        {
            //emitter->SetMaterial(ribbonMaterial);
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

    m_GraphicsContext->SetClearColor({ 0.015f, 0.025f, 0.06f, 1.0f });

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
}

void Dissolve::Prepare(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::Prepare(frameTime);
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

void Dissolve::UpdateAnimation(const float deltaSeconds)
{
    const auto& instanceMesh = m_MeshInstance.GetMesh();
    if (!instanceMesh) return;

    const auto& animations = instanceMesh->GetAnimations();
    if (animations.empty()) return;

    constexpr float animationInterval = 2.0f;
    constexpr float fadeDuration = 0.6f;
    float remaining = deltaSeconds;

    // Split updates at each switch so transitions keep the same timing at any frame rate.
    while (remaining > 0.0f)
    {
        const auto step = std::min(remaining, animationInterval - m_AnimationTime);
        if (!m_MeshInstance.Update(step)) return;

        remaining -= step;
        m_AnimationTime += step;
        if (m_AnimationTime < animationInterval) continue;

        m_AnimationTime = 0.0f;
        const auto next = (m_AnimationIndex + 1) % animations.size();
        if (animations.size() > 1 && m_MeshInstance.Play(animations[next], fadeDuration))
        {
            EE_CORE_INFO("Animation demo: '{}' -> '{}' over {} seconds.",
                animations[m_AnimationIndex]->GetName(), animations[next]->GetName(), fadeDuration)
            m_AnimationIndex = next;
        }
    }
}

void Dissolve::DrawGeometry()
{
    if (m_SkeletalMeshRenderer && m_MeshInstance.GetMesh())
    {
        // Show the demo from an angle that keeps the head, tail, and legs visible.
        const auto& bounds = m_MeshInstance.GetMesh()->GetLocalBounds();
        const auto center = (bounds.Min + bounds.Max) * 0.5f;
        const auto transform = glm::translate(glm::mat4(1), center) *
            glm::rotate(glm::mat4(1), glm::radians(135.0f), glm::vec3(0, 1, 0)) *
            glm::translate(glm::mat4(1), -center);
        m_SkeletalMeshRenderer->Render(m_MeshInstance, transform);
    }
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
