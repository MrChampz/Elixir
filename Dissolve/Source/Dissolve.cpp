#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Aether/Renderer.h>

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

    m_ParticleSystem = CreateScope<Aether::System>("Ribbon Garden");
    m_ParticleSystem->GetParameters().SetFloat("GravityScale", 1.0f);
    m_ParticleSystem->GetParameters().SetFloat4("CanopyColorStart", { 0.74f, 0.94f, 1.0f, 0.74f });
    m_ParticleSystem->GetParameters().SetFloat4("CanopyColorEnd", { 0.2f, 0.46f, 0.88f, 0.0f });
    m_ParticleSystem->GetParameters().SetFloat4("SparkColorStart", { 0.98f, 0.62f, 1.0f, 0.92f });
    m_ParticleSystem->GetParameters().SetFloat4("SparkColorEnd", { 0.44f, 0.1f, 0.78f, 0.0f });

    auto& ribbon = m_ParticleSystem->AddEmitter("PathRibbon", 240, 90.0f);
    ribbon.SetRenderMode(Aether::EParticleRenderMode::Ribbon);
    ribbon.AddSpawnModule<Aether::SetPositionOnCircle>(glm::vec2{ 0.0f, 0.0f }, 2.0f, 1.0f);
    ribbon.AddSpawnModule<Aether::SetLifetime>(2.2f, 2.8f);
    ribbon.AddSpawnModule<Aether::SetSize>(14.0f, 20.0f);
    ribbon.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.74f, 0.94f, 1.0f, 0.74f });

    ribbon.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.74f, 0.94f, 1.0f, 0.74f }, glm::vec4{ 0.2f, 0.46f, 0.88f, 0.0f });
    ribbon.AddUpdateModule<Aether::SizeOverLife>(18.0f, 3.0f);
    ribbon.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec2{ -1.45f, -1.2f }, glm::vec2{ 1.45f, 1.35f });

    auto& fountain = m_ParticleSystem->AddEmitter("PurpleFountain", 1600, 120.0f);
    fountain.AddSpawnModule<Aether::SetPositionCircle>(glm::vec2{ 0.0f, -0.86f }, 0.07f);
    fountain.AddSpawnModule<Aether::SetVelocityCone>(1.18f, 1.96f, 0.34f, 0.82f);
    fountain.AddSpawnModule<Aether::SetLifetime>(1.8f, 2.9f);
    fountain.AddSpawnModule<Aether::SetSize>(7.0f, 14.0f);
    fountain.AddSpawnModule<Aether::SetColor>(glm::vec4{ 0.98f, 0.62f, 1.0f, 0.92f });

    fountain.AddUpdateModule<Aether::ApplyGravity>(glm::vec2{ 0.0f, -0.3f });
    fountain.AddUpdateModule<Aether::ApplyLinearDrag>(0.08f);
    fountain.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 0.98f, 0.62f, 1.0f, 0.92f }, glm::vec4{ 0.44f, 0.1f, 0.78f, 0.0f });
    fountain.AddUpdateModule<Aether::SizeOverLife>(14.0f, 2.0f);
    fountain.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec2{ -1.45f, -1.2f }, glm::vec2{ 1.45f, 1.35f });

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