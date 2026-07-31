#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Aether/Renderer.h>
#include <Engine/Aether/Effect.h>

#include <Engine/Material/MaterialGraph.h>
#include <Engine/Material/MaterialInstance.h>
#include <Engine/Material/MaterialCompiler.h>

Ref<GraphicsPipeline> pipeline;
Scope<Aether::Renderer> m_ParticlesRenderer;
Aether::FrameSubmission m_ParticleFrameSubmission;
std::array<Ref<Aether::System>, 2> m_ParticleSystems;
std::array<Scope<Aether::SystemInstance>, 2> m_ParticleSystemInstances;

Ref<Material> graphMaterial;
Ref<const SCompiledMaterial> compiledGraphMaterial;

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

    {
        MaterialGraph graph;

        graphMaterial = CreateRef<Material>("DissolveGraph");
        EE_CORE_ASSERT(
            graphMaterial->SetUsage(EMaterialUsage::ParticleSprite, true),
            "Dissolve graph material must enable ParticleSprite usage."
        )

        EE_CORE_ASSERT(graphMaterial->DefineParameter("Tint", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialGraphValueType::Float4,
            .DefaultValue = SMaterialParam::MakeVector({ 1.0f, 0.5f, 0.2f, 1.0f }),
        }), "")

        EE_CORE_ASSERT(graphMaterial->DefineParameter("Albedo", {
            .Kind = EMaterialParameterKind::Texture,
            .DefaultValue = SMaterialParam::MakeTexture(tex),
        }), "")

        SMaterialNode albedo;
        albedo.Type = EMaterialNodeType::TextureSample;
        albedo.TextureParameterName = "Albedo";
        const auto albedoNode = graph.AddNode(albedo);

        SMaterialNode tint;
        tint.Type = EMaterialNodeType::Parameter;
        tint.OutputType = EMaterialGraphValueType::Float4;
        tint.ParameterName = "Tint";
        const auto tintNode = graph.AddNode(tint);

        SMaterialNode baseColor;
        baseColor.Type = EMaterialNodeType::Multiply;
        baseColor.OutputType = EMaterialGraphValueType::Float4;
        baseColor.Inputs = {
            (int32_t)albedoNode,
            (int32_t)tintNode,
        };
        graph.SetChannel(EMaterialChannel::BaseColor, graph.AddNode(baseColor));

        graphMaterial->SetGraph(std::move(graph));

        const auto result = MaterialCompiler::Compile(m_ShaderLoader.get(), *graphMaterial);

        if (result)
        {
            compiledGraphMaterial = result.Material;

            auto instance = CreateRef<MaterialInstance>(graphMaterial);
            EE_CORE_ASSERT(
                instance->SetVector("Tint", { 1.0f, 0.35f, 0.1f, 1.0f }),
                "Dissolve graph material tint override must match its schema."
            )

            const auto proxy = instance->CreateRenderProxy(compiledGraphMaterial);
            EE_CORE_ASSERT(
                proxy,
                "Dissolve graph material render proxy must match the compiled schema."
            )

            if (auto* emitter = m_ParticleSystems[0]->FindEmitter("FlameCore"))
            {
                emitter->SetMaterial(proxy);
                EE_CORE_INFO("Published graph material to the FlameCore particle emitter.")
            }
            else
            {
                EE_CORE_ERROR("Dissolve particle emitter 'FlameCore' was not found.")
            }
        }
        else
            EE_CORE_ERROR("Node-graph material compilation failed: {}", result.Diagnostics)
    }

    {
        MaterialGraph graph1;

        const auto ribbonMaterial = CreateRef<Material>("RibbonEnergy");
        EE_CORE_ASSERT(
            ribbonMaterial->SetUsage(EMaterialUsage::ParticleRibbon, true),
            "Ribbon material must enable ParticleRibbon usage."
        )
        EE_CORE_ASSERT(
            ribbonMaterial->SetUsage(EMaterialUsage::ParticleMesh, true),
            "Ribbon material must enable ParticleRibbon usage."
        )

        EE_CORE_ASSERT(ribbonMaterial->DefineParameter("Tint", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialGraphValueType::Float4,
            .DefaultValue = SMaterialParam::MakeVector({ 0.2f, 0.5f, 1.0f, 1.0f }),
        }), "")

        EE_CORE_ASSERT(ribbonMaterial->DefineParameter("Glow", {
            .Kind = EMaterialParameterKind::Value,
            .ValueType = EMaterialGraphValueType::Float4,
            .DefaultValue = SMaterialParam::MakeVector({ 0.05f, 0.2f, 1.0f, 1.0f }),
        }), "")

        EE_CORE_ASSERT(ribbonMaterial->DefineParameter("Albedo", {
            .Kind = EMaterialParameterKind::Texture,
            .DefaultValue = SMaterialParam::MakeTexture(tex),
        }), "")

        SMaterialNode panner;
        panner.Type = EMaterialNodeType::Panner;
        panner.OutputType = EMaterialGraphValueType::Float2;
        panner.ConstantValue = { 0.08f, -0.35f, 0.0f, 0.0f };
        const auto pannerNode = graph1.AddNode(panner);

        SMaterialNode albedo1;
        albedo1.Type = EMaterialNodeType::TextureSample;
        albedo1.OutputType = EMaterialGraphValueType::Float3;
        albedo1.TextureParameterName = "Albedo";
        albedo1.Inputs = { static_cast<int32_t>(pannerNode) };
        const auto albedoNode1 = graph1.AddNode(albedo1);

        SMaterialNode tint1;
        tint1.Type = EMaterialNodeType::Parameter;
        tint1.OutputType = EMaterialGraphValueType::Float4;
        tint1.ParameterName = "Tint";
        const auto tintNode1 = graph1.AddNode(tint1);

        SMaterialNode color;
        color.Type = EMaterialNodeType::Multiply;
        color.OutputType = EMaterialGraphValueType::Float4;
        color.Inputs = {
            static_cast<int32_t>(albedoNode1),
            static_cast<int32_t>(tintNode1),
        };
        graph1.SetChannel(EMaterialChannel::BaseColor, albedoNode1);

        SMaterialNode glow;
        glow.Type = EMaterialNodeType::Parameter;
        glow.OutputType = EMaterialGraphValueType::Float4;
        glow.ParameterName = "Glow";
        //graph1.SetChannel(EMaterialChannel::Emissive, graph1.AddNode(glow));

        ribbonMaterial->SetGraph(std::move(graph1));

        const auto compileResult = MaterialCompiler::Compile(
            m_ShaderLoader.get(),
            *ribbonMaterial
        );
        EE_CORE_ASSERT(compileResult, "Ribbon material compilation failed.")

        const auto instance = CreateRef<MaterialInstance>(ribbonMaterial);
        EE_CORE_ASSERT(
            instance->SetVector("Tint", { 0.15f, 0.6f, 1.0f, 1.0f }),
            "Ribbon tint override must match the schema."
        )

        const auto proxy = instance->CreateRenderProxy(compileResult.Material);
        EE_CORE_ASSERT(proxy, "Ribbon material proxy creation failed.")

        if (auto* emitter = m_ParticleSystems[1]->FindEmitter("PathRibbon"))
        {
            emitter->SetMaterial(proxy);
            EE_CORE_INFO("Published graph material to the PathRibbon particle emitter.")
        }

        if (auto* emitter = m_ParticleSystems[1]->FindEmitter("CrystalShards"))
        {
            emitter->SetMaterial(proxy);
            EE_CORE_INFO("Published graph material to the CrystalShards particle emitter.")
        }
    }

    m_ParticleSystemInstances[0] = CreateScope<Aether::SystemInstance>(
        CreateRef<Aether::SCompiledSystem>(m_ParticleSystems[0]->Compile())
    );

    m_ParticleSystemInstances[1] = CreateScope<Aether::SystemInstance>(
        CreateRef<Aether::SCompiledSystem>(m_ParticleSystems[1]->Compile())
    );

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