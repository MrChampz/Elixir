#include "Dissolve.h"

#include "Engine/Graphics/Shader/ShaderLoader.h"

#include <Engine/Core/Entrypoint.h>

Ref<GraphicsPipeline> pipeline;

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");
    m_DrawExtent = { m_Window->GetWidth(), m_Window->GetHeight() };

    const auto tex = TextureLoader::Load("./Assets/Bricks.png");

    const auto loader = CreateScope<ShaderLoader>(m_GraphicsContext.get());

    //const auto shader = loader->LoadShader("./Shaders/", "Basic");
    //shader->GetName();

    const auto shader1 = loader->LoadShader("./Shaders/", "FixedTriangle");
    //shader1->BindTexture("texture", tex);

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
    builder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
    builder.SetBufferLayout({});
    pipeline = builder.Build(m_GraphicsContext.get());
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

    // begin command buffer
    const auto cmd = m_GraphicsContext->GetCommandBuffer();
    cmd->Begin();

    float flash = std::abs(std::sin(m_GraphicsContext->GetFrameNumber() / 120.f));

    m_GraphicsContext->SetClearColor({ 0.0f, 0.0f, flash, 1.0 });
    m_GraphicsContext->Clear();

    DrawGeometry(cmd);
}

void Dissolve::DrawGeometry(const Ref<CommandBuffer>& cmd)
{
    cmd->BeginRendering(
        m_GraphicsContext->GetRenderTarget(),
        nullptr,
        m_DrawExtent
    );
    
    Viewport viewport = {};
    viewport.X = 0;
    viewport.Y = 0;
    viewport.Width = m_DrawExtent.Width;
    viewport.Height = m_DrawExtent.Height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    cmd->SetViewports({ viewport });

    Rect2D scissor = {};
    scissor.Offset = { 0, 0 };
    scissor.Extent = m_DrawExtent;

    cmd->SetScissors({ scissor });

    pipeline->Bind(cmd);

    cmd->Draw(3);

    cmd->EndRendering();
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
