#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Aether/Renderer.h>

#include <array>
#include <utility>
#include <vector>

Ref<GraphicsPipeline> pipeline;
Scope<Aether::Renderer> m_ParticlesRenderer;
Scope<Aether::System> m_ParticleSystem;
Aether::SGPUSystem m_GPUSystem;

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");
    m_DrawExtent = m_Window->GetFramebufferExtent();

    const auto aspectRatio = (float)m_DrawExtent.Width / (float)m_DrawExtent.Height;
    // m_CameraController = CreateScope<SplineCameraController>(aspectRatio);
    // m_CameraController->AddKeyframe({ { 0.0f, 0.5f, 5.0f }, { 0.0f, 0.0f, 0.0f }, 60.0f });
    // m_CameraController->AddKeyframe({ { 1.0f, 0.3f, 2.5f }, { 0.0f, 0.0f, 0.0f }, 50.0f });
    // m_CameraController->AddKeyframe({ { 0.0f, 0.0f, 1.5f }, { 0.0f, 0.0f, 0.0f }, 40.0f });
    // m_CameraController->SetDuration(4.0f);
    // m_CameraController->SetLooping(true);
    // m_CameraController->Play();
    m_CameraController = CreateScope<ArcBallCameraController>(60.0f, aspectRatio);
    m_FrameData.ViewProj = m_CameraController->GetCamera().GetViewProjectionMatrix();

    const auto sampler = SamplerBuilder()
        .Build(m_GraphicsContext.get());

    const auto tex = TextureLoader::Load("./Assets/Bricks.png");

    const auto shader = m_ShaderLoader->LoadShader("./Shaders/", "FixedTriangle");
    shader->BindTexture("texture", tex);
    shader->BindSampler("sampl", sampler);

    PipelineBuilder builder;
    builder.SetShader(shader);
    builder.SetInputTopology(EPrimitiveTopology::TriangleList);
    builder.SetPolygonMode(EPolygonMode::Fill);
    builder.DisableBlending();
    builder.DisableDepthTest();
    builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
    builder.SetBufferLayout({});
    pipeline = builder.Build(m_GraphicsContext.get());

    m_FrameConstantBuffer = UniformBuffer::Create(
        m_GraphicsContext.get(),
        sizeof(SFrameData),
        &m_FrameData
    );

    shader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

    m_ParticlesRenderer = CreateScope<Aether::Renderer>(m_GraphicsContext.get(), m_ShaderLoader.get());

    m_ParticleSystem = CreateScope<Aether::System>("Aurora Garden");
    m_ParticleSystem->GetParameters().SetFloat("GravityScale", 1.0f);
    m_ParticleSystem->GetParameters().SetFloat4("CanopyColorStart", { 0.70f, 0.96f, 1.0f, 0.78f });
    m_ParticleSystem->GetParameters().SetFloat4("CanopyColorEnd", { 0.40f, 0.18f, 0.72f, 0.0f });
    m_ParticleSystem->GetParameters().SetFloat4("SparkColorStart", { 1.0f, 0.72f, 0.90f, 0.95f });
    m_ParticleSystem->GetParameters().SetFloat4("SparkColorEnd", { 1.0f, 0.36f, 0.48f, 0.0f });

    auto& canopy = m_ParticleSystem->AddEmitter("CanopyMist", 4096, 160.0f);
    canopy.AddSpawnModule<Aether::SetPositionCircle>(glm::vec2{ 0.0f, -0.86f }, 0.18f);
    canopy.AddSpawnModule<Aether::SetVelocityCone>(1.26f, 1.88f, 0.24f, 0.56f);
    canopy.AddSpawnModule<Aether::SetLifetime>(3.8f, 6.2f);
    canopy.AddSpawnModule<Aether::SetSize>(14.0f, 28.0f);
    canopy.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.70f, 0.96f, 1.0f, 0.72f });

    canopy.AddUpdateModule<Aether::ApplyGravity>(glm::vec2{ 0.0f, -0.10f });
    canopy.AddUpdateModule<Aether::ApplyLinearDrag>(0.04f);
    canopy.AddUpdateModule<Aether::IntegrateVelocity>();
    canopy.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.70f, 0.96f, 1.0f, 0.78f }, glm::vec4{ 0.4f, 0.18f, 0.72f, 0.0f });
    canopy.AddUpdateModule<Aether::SizeOverLife>(28.0f, 6.0f);
    canopy.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec2{ -1.45f, -1.2f }, glm::vec2{ 1.45f, 1.35f });

    auto& sparks = m_ParticleSystem->AddEmitter("RoseSparks", 2048, 120.0f);
    sparks.AddSpawnModule<Aether::SetPositionCircle>(glm::vec2{ 0.0f, -0.8f }, 0.08f);
    sparks.AddSpawnModule<Aether::SetVelocityCone>(1.08f, 2.04f, 0.36f, 0.84f);
    sparks.AddSpawnModule<Aether::SetLifetime>(1.8f, 3.0f);
    sparks.AddSpawnModule<Aether::SetSize>(8.0f, 16.0f);
    sparks.AddSpawnModule<Aether::SetColor>(glm::vec4{ 1.0f, 0.68f, 0.88f, 0.95f });

    sparks.AddUpdateModule<Aether::ApplyGravity>(glm::vec2{ 0.0f, -0.28f });
    sparks.AddUpdateModule<Aether::ApplyLinearDrag>(0.08f);
    sparks.AddUpdateModule<Aether::IntegrateVelocity>();
    sparks.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 1.0f, 0.72f, 0.9f, 0.95f }, glm::vec4{ 1.0f, 0.36f, 0.48f, 0.0f });
    sparks.AddUpdateModule<Aether::SizeOverLife>(16.0f, 2.5f);
    sparks.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec2{ -1.45f, -1.2f }, glm::vec2{ 1.45f, 1.35f });

    constexpr uint32_t ribbonParticles = 256;
    constexpr float ribbonLifetime = 4.0f;
    constexpr float ribbonSpawnRate = (float)ribbonParticles / ribbonLifetime;

    auto& ribbon = m_ParticleSystem->AddEmitter("AuroraRibbon", ribbonParticles, ribbonSpawnRate);
    ribbon.SetRenderMode(Aether::EParticleRenderMode::Ribbon);
    ribbon.SetRibbonWidthScale(0.005f);

    const glm::vec2 ribbonCenter{ 0.0f, 0.04f };
    const std::array<glm::vec2, 7> ribbonAnchors{
        ribbonCenter + glm::vec2{ -0.18f,  0.55f },
        ribbonCenter + glm::vec2{  0.46f,  0.46f },
        ribbonCenter + glm::vec2{  0.70f,  0.04f },
        ribbonCenter + glm::vec2{  0.32f, -0.42f },
        ribbonCenter + glm::vec2{ -0.28f, -0.50f },
        ribbonCenter + glm::vec2{ -0.66f, -0.12f },
        ribbonCenter + glm::vec2{ -0.52f,  0.36f }
    };

    std::vector<Aether::SBezierCurve> ribbonPath;
    ribbonPath.reserve(ribbonAnchors.size());

    constexpr float bezierTension = 1.0f;
    for (size_t i = 0; i < ribbonAnchors.size(); ++i)
    {
        const auto previous = ribbonAnchors[(i + ribbonAnchors.size() - 1) % ribbonAnchors.size()];
        const auto start = ribbonAnchors[i];
        const auto end = ribbonAnchors[(i + 1) % ribbonAnchors.size()];
        const auto next = ribbonAnchors[(i + 2) % ribbonAnchors.size()];

        const auto controlA = start + ((end - previous) * (bezierTension / 6.0f));
        const auto controlB = end - ((next - start) * (bezierTension / 6.0f));
        ribbonPath.push_back({ start, controlA, controlB, end });
    }

    ribbon.AddSpawnModule<Aether::SetPositionBezierLoop>(std::move(ribbonPath), ribbonLifetime);
    ribbon.AddSpawnModule<Aether::SetLifetime>(ribbonLifetime, ribbonLifetime);
    ribbon.AddSpawnModule<Aether::SetSize>(8.0f, 8.0f);
    ribbon.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.55f, 0.92f, 1.0f, 0.95f });

    ribbon.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.55f, 0.92f, 1.0f, 0.95f }, glm::vec4{ 0.78f, 0.36f, 1.0f, 0.0f });

    m_GPUSystem = m_ParticleSystem->Build();

    m_GraphicsContext->SetClearColor({ 0.015f, 0.025f, 0.06f, 1.0f });
}

Dissolve::~Dissolve()
{
    pipeline.reset();
}

void Dissolve::OnGUI(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnGUI(frameTime);
}

void Dissolve::OnRender(const Timestep frameTime)
{
    EE_PROFILE_ZONE_SCOPED()
    Application::OnRender(frameTime);

    m_CameraController->Update(frameTime);
    m_FrameData.ViewProj = m_CameraController->GetCamera().GetViewProjectionMatrix();
    m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));
    m_ParticleSystem->Update(frameTime);
    m_ParticlesRenderer->Update(frameTime);

    m_GraphicsContext->Clear();

    //DrawGeometry();

    m_ParticlesRenderer->Render(m_GPUSystem, m_CameraController->GetCamera());
}

void Dissolve::OnEvent(Event& event)
{
    Application::OnEvent(event);
    m_CameraController->ProcessEvent(event);
}

void Dissolve::DrawGeometry()
{
     const auto renderingInfo = SRenderingInfo
     {
         .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
         .RenderArea = m_DrawExtent
     };

     Viewport viewport = {};
     viewport.X = 0;
     viewport.Y = 0;
     viewport.Width = m_DrawExtent.Width;
     viewport.Height = m_DrawExtent.Height;
     viewport.MinDepth = 0.0f;
     viewport.MaxDepth = 1.0f;

     Rect2D scissor = {};
     scissor.Offset = { 0, 0 };
     scissor.Extent = m_DrawExtent;

     m_Executor.Enqueue([this, renderingInfo, viewport, scissor]()
     {
         const auto cmd = this->m_GraphicsContext->GetSecondaryCommandBuffer();
         cmd->BeginRendering(renderingInfo);
         cmd->SetViewports({ viewport });
         cmd->SetScissors({ scissor });
         pipeline->Bind(cmd);
         cmd->Draw(3);
         cmd->EndRendering();
         this->m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
     }, &m_WaitGroup);

    m_WaitGroup.Wait();
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
