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

    m_ParticleSystem = CreateScope<Aether::System>("Fountain");
    m_ParticleSystem->GetParameters().SetFloat("GravityScale", 1.0f);

    auto& emitter = m_ParticleSystem->AddEmitter("Emitter", 4096, 240.0f);
    emitter.AddSpawnModule<Aether::SetPositionCircle>(glm::vec2{ 0.0f, -0.78f }, 0.03f);
    emitter.AddSpawnModule<Aether::SetVelocityCone>(1.1f, 2.05f, 0.45f, 0.95f);
    emitter.AddSpawnModule<Aether::SetLifetime>(1.2f, 2.5f);
    emitter.AddSpawnModule<Aether::SetSize>(6.0f, 14.0f);
    emitter.AddSpawnModule<Aether::SetColor>(glm::vec4{ 1.0f, 0.75f, 0.25f, 1.0f });

    emitter.AddUpdateModule<Aether::ApplyGravity>(glm::vec2{ 0.0f, -0.9f });
    emitter.AddUpdateModule<Aether::ApplyLinearDrag>(0.18f);
    emitter.AddUpdateModule<Aether::IntegrateVelocity>();
    emitter.AddUpdateModule<Aether::ColorOverLife>(glm::vec4{ 1.0f, 0.85f, 0.3f, 0.95f }, glm::vec4{ 0.2f, 0.45f, 1.0f, 0.0f });
    emitter.AddUpdateModule<Aether::SizeOverLife>(14.0f, 2.0f);
    emitter.AddUpdateModule<Aether::KillOutsideBounds>(glm::vec2{ -1.25f, -1.25f }, glm::vec2{ 1.25f, 1.25f });

    m_GPUSystem = m_ParticleSystem->Build();
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

    float flash = std::abs(std::sin(m_GraphicsContext->GetFrameNumber() / 120.f));

    m_GraphicsContext->SetClearColor({ 0.0f, 0.0f, flash, 1.0 });
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