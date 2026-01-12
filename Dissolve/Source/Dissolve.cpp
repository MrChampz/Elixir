#include "Dissolve.h"

#include <Engine/Core/Entrypoint.h>

Ref<GraphicsPipeline> pipeline;

void ThreadsTest(Executor& executor)
{
    constexpr int NUM_TASKS = 1000;

    std::cout << "Starting thread test with " << NUM_TASKS << " tasks.." << std::endl;

    const auto start = std::chrono::high_resolution_clock::now();

    auto wg = WaitGroup();

    for (int i = 0; i < NUM_TASKS; ++i)
    {
        executor.Enqueue([]()
        {
            EE_PROFILE_ZONE_SCOPED()
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(10, 100);
        }, &wg);
    }

    wg.Wait();

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTest completed!" << std::endl;
    std::cout << "Total time: " << duration.count() << "ms" << std::endl;
    std::cout << "Average time per task: " << duration.count() / (double)NUM_TASKS << "ms" << std::endl;
    std::cout << "Tasks per second: " << (NUM_TASKS * 1000.0) / duration.count() << std::endl;
}

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");
    m_DrawExtent = { m_Window->GetWidth(), m_Window->GetHeight() };

    const auto tex = TextureLoader::Load("./Assets/Bricks.png");

    //const auto shader = loader->LoadShader("./Shaders/", "Basic");
    //shader->GetName();

    const auto shader1 = m_ShaderLoader->LoadShader("./Shaders/", "FixedTriangle");
    shader1->BindTexture("texture", tex);

    // BufferLayout bufferLayout({
    //     { EDataType::Vec3, "Pos" },
    //     { EDataType::Vec2, "Tex" }
    // });

    PipelineBuilder builder;
    builder.SetShader(shader1);
    builder.SetInputTopology(EPrimitiveTopology::TriangleList);
    builder.SetPolygonMode(EPolygonMode::Fill);
    builder.DisableBlending();
    builder.DisableDepthTest();
    builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
    builder.SetBufferLayout({});
    pipeline = builder.Build(m_GraphicsContext.get());

    ThreadsTest(m_Executor);
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

    float flash = std::abs(std::sin(m_GraphicsContext->GetFrameNumber() / 120.f));

    m_GraphicsContext->SetClearColor({ 0.0f, 0.0f, flash, 1.0 });
    m_GraphicsContext->Clear();

    DrawGeometry();
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