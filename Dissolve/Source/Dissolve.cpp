#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Aether/Renderer.h>

#include "Engine/Aether/Effect.h"

Ref<GraphicsPipeline> pipeline;
Scope<Aether::Renderer> m_ParticlesRenderer;
Ref<Aether::System> m_ParticleSystem;
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

    m_ParticleSystem = Aether::LoadEffectFile("./Assets/VFX/FireAndFireworks.json");
    // m_ParticleSystem = CreateScope<Aether::System>("Ribbon Garden");
    // m_ParticleSystem->GetParameters().SetFloat("GravityScale", 1.0f);
    //
    // auto& canopy = m_ParticleSystem->AddEmitter("CanopyMist", 4096, 160.0f);
    // canopy.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.86f, 0.0f }, 0.18f);
    // canopy.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 0.62f, 0.24f, 0.56f);
    // canopy.AddSpawnModule<Aether::SetLifetime>(3.8f, 6.2f);
    // canopy.AddSpawnModule<Aether::SetSize>(14.0f, 28.0f);
    // canopy.AddSpawnModule<Aether::SetScale>(0.72f, 1.08f);
    // canopy.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.70f, 0.96f, 1.0f, 0.72f });
    //
    // canopy.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.10f, 0.0f });
    // canopy.AddUpdateModule<Aether::ApplyLinearDrag>(0.04f);
    // canopy.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.70f, 0.96f, 1.0f, 0.78f }, glm::vec4{ 0.4f, 0.18f, 0.72f, 0.0f });
    // canopy.AddUpdateModule<Aether::SizeOverLife>(28.0f, 6.0f);
    // canopy.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -2.0f }, glm::vec3{ 1.45f, 1.35f, 2.0f });
    //
    // auto& sparks = m_ParticleSystem->AddEmitter("RoseSparks", 2048, 120.0f);
    // sparks.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.8f, 0.0f }, 0.08f);
    // sparks.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 0.96f, 0.36f, 0.84f);
    // sparks.AddSpawnModule<Aether::SetLifetime>(1.8f, 3.0f);
    // sparks.AddSpawnModule<Aether::SetSize>(8.0f, 16.0f);
    // sparks.AddSpawnModule<Aether::SetColor>(glm::vec4{ 1.0f, 0.68f, 0.88f, 0.95f });
    //
    // sparks.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.28f, 0.0f });
    // sparks.AddUpdateModule<Aether::ApplyLinearDrag>(0.08f);
    // sparks.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 1.0f, 0.72f, 0.9f, 0.95f }, glm::vec4{ 1.0f, 0.36f, 0.48f, 0.0f });
    // sparks.AddUpdateModule<Aether::SizeOverLife>(16.0f, 2.5f);
    // sparks.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -2.0f }, glm::vec3{ 1.45f, 1.35f, 2.0f });
    //
    // auto& sparks2 = m_ParticleSystem->AddEmitter("GreenSparks", 2048, 120.0f);
    // sparks2.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.8f, 0.0f }, 0.08f);
    // sparks2.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 0.96f, 0.36f, 0.84f);
    // sparks2.AddSpawnModule<Aether::SetLifetime>(1.8f, 3.0f);
    // sparks2.AddSpawnModule<Aether::SetSize>(8.0f, 16.0f);
    // sparks2.AddSpawnModule<Aether::SetScale>(0.9f, 1.35f);
    // sparks2.AddSpawnModule<Aether::SetColor>(glm::vec4{ 1.0f, 0.68f, 0.88f, 0.95f });
    //
    // sparks2.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.28f, 0.0f });
    // sparks2.AddUpdateModule<Aether::ApplyLinearDrag>(0.08f);
    // sparks2.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.0f, 0.73f, 0.12f, 0.8f }, glm::vec4{ 0.0f, 0.73f, 0.12f, 0.0f });
    // sparks2.AddUpdateModule<Aether::SizeOverLife>(16.0f, 2.5f);
    // sparks2.AddUpdateModule<Aether::ScaleOverLife>(1.0f, 0.35f);
    // sparks2.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -2.0f }, glm::vec3{ 1.45f, 1.35f, 2.0f });
    //
    // constexpr uint32_t ribbonParticles = 256;
    // constexpr float ribbonLifetime = 4.0f;
    // constexpr float ribbonSpawnRate = (float)ribbonParticles / ribbonLifetime;
    //
    // auto& ribbon = m_ParticleSystem->AddEmitter("AuroraRibbon", ribbonParticles, ribbonSpawnRate);
    // ribbon.SetRenderMode(Aether::EParticleRenderMode::Ribbon);
    // // ribbon.AddSpawnModule<Aether::SetPositionCircularPath>(
    // //     glm::vec3{ 0.0f, -0.25f, 0.0f },
    // //     glm::vec3{ 1.05f, 0.38f, 0.28f },
    // //     glm::vec3{ 0.32f, 0.16f, 0.18f },
    // //     1.15f
    // // );
    // ribbon.AddSpawnModule<Aether::SetPositionOnCircle>(glm::vec3{ 0.0f, 0.0f, 0.0f }, 2.0f, 1.0f);
    // ribbon.AddSpawnModule<Aether::SetLifetime>(ribbonLifetime, ribbonLifetime);
    // ribbon.AddSpawnModule<Aether::SetSize>(8.0f, 8.0f);
    // ribbon.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.55f, 0.92f, 1.0f, 0.95f });
    //
    // ribbon.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.55f, 0.92f, 1.0f, 0.95f }, glm::vec4{ 0.78f, 0.36f, 1.0f, 0.0f });
    //
    // auto& shards = m_ParticleSystem->AddEmitter("CrystalShards", 220, 26.0f);
    // shards.SetRenderMode(Aether::EParticleRenderMode::Mesh);
    // shards.AddSpawnModule<Aether::SetPositionDisk>(glm::vec3{ 0.0f, -0.64f, 0.0f }, 0.22f);
    // shards.AddSpawnModule<Aether::SetVelocityCone>(glm::vec3{ 0.94f }, 2.20f, 0.14f, 0.36f);
    // shards.AddSpawnModule<Aether::SetLifetime>(2.6f, 3.8f);
    // shards.AddSpawnModule<Aether::SetSize>(10.0f, 18.0f);
    // shards.AddSpawnModule<Aether::SetScale>(0.82f, 1.45f);
    // shards.AddSpawnModule<Aether::SetRotation>(0.0f, 6.28318530718f);
    // shards.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.86f, 0.94f, 1.0f, 0.62f });
    //
    // shards.AddUpdateModule<Aether::ApplyGravity>(glm::vec3{ 0.0f, -0.28f, 0.0f });
    // shards.AddUpdateModule<Aether::ApplyLinearDrag>(0.03f);
    // shards.AddUpdateModule<Aether::ApplyAngularVelocity>(1.2f);
    // shards.AddUpdateModule<Aether::SizeOverLife>(18.0f, 3.0f);
    // shards.AddUpdateModule<Aether::ScaleOverLife>(1.15f, 0.28f);
    // shards.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec3{ -1.45f, -1.2f, -1.45f }, glm::vec3{ 1.45f, 1.35f, 1.45f });

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

    m_ParticlesRenderer->Update(frameTime);

    m_GraphicsContext->Clear();

    //DrawGeometry();

    m_ParticlesRenderer->Render(m_GPUSystem, m_CameraController->GetCamera());
    const auto& metrics = m_ParticlesRenderer->GetLastSubmissionMetrics();
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