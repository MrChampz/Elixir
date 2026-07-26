#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Aether/Renderer.h>
#include <Engine/Aether/Effect.h>

#include <Engine/Material/MaterialGraph.h>
#include <Engine/Material/MaterialCompiler.h>

Ref<GraphicsPipeline> pipeline;
Scope<Aether::Renderer> m_ParticlesRenderer;
Aether::FrameSubmission m_ParticleFrameSubmission;
std::array<Ref<Aether::System>, 2> m_ParticleSystems;
std::array<Scope<Aether::SystemInstance>, 2> m_ParticleSystemInstances;

Ref<Shader> graphShader;

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

    m_ParticleSystems[0] = Aether::LoadEffectFile("./Assets/VFX/FireAndFireworks.json");
    m_ParticleSystems[1] = Aether::LoadEffectFile("./Assets/VFX/RibbonVortex.json");

    m_ParticleSystemInstances[0] = CreateScope<Aether::SystemInstance>(CreateRef<Aether::SCompiledSystem>(m_ParticleSystems[0]->Compile()));
    m_ParticleSystemInstances[1] = CreateScope<Aether::SystemInstance>(CreateRef<Aether::SCompiledSystem>(m_ParticleSystems[1]->Compile()));

    {
        MaterialGraph graph;

        SMaterialNode baseColor;
        baseColor.Type = EMaterialNodeType::Constant;
        baseColor.OutputType = EMaterialGraphValueType::Float4;
        baseColor.ConstantValue = { 0.9f, 0.3f, 0.1f, 1.0f };
        graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode(baseColor));

        SMaterialNode metallic;
        metallic.Type = EMaterialNodeType::Constant;
        metallic.OutputType = EMaterialGraphValueType::Float;
        metallic.ConstantValue = { 0.9f, 0.0f, 0.0f, 0.0f };
        graph.SetChannel(EMaterialChannel::Metallic, graph.AddNode(metallic));

        graphShader = MaterialCompiler::Compile(m_ShaderLoader.get(), graph);
        if (graphShader)
            EE_CORE_INFO("Node-graph material compiled and loaded successfully.")
        else
            EE_CORE_ERROR("Node-graph material compilation FAILED.")
    }

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

    m_ParticleFrameSubmission.Reset();

    bool submitted = m_ParticleFrameSubmission.Submit(*m_ParticleSystemInstances[0]);
    EE_CORE_ASSERT(submitted, "The particle system instance was submitted more than once.")

    submitted = m_ParticleFrameSubmission.Submit(*m_ParticleSystemInstances[1]);
    EE_CORE_ASSERT(submitted, "The particle system instance was submitted more than once.")

    m_ParticlesRenderer->Render(m_ParticleFrameSubmission, m_CameraController->GetCamera());
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