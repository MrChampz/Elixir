#include "Dissolve.h"

#include "Engine/Graphics/Shader/ShaderLoader.h"

#include <Engine/Core/Entrypoint.h>

Dissolve::Dissolve()
{
    EE_PROFILE_ZONE_SCOPED()

    m_Window->SetTitle("Dissolve");

    const auto tex = TextureLoader::Load("./Assets/Bricks.png");

    const auto loader = CreateScope<ShaderLoader>(m_GraphicsContext.get());

    //const auto shader = loader->LoadShader("./Shaders/", "Basic");
    //shader->GetName();

    const auto shader1 = loader->LoadShader("./Shaders/", "Texture");
    shader1->BindTexture("texture", tex);
    shader1->GetName();

    BufferLayout bufferLayout({
        { EDataType::Vec3, "Pos" },
        { EDataType::Vec2, "Tex" }
    });

    PipelineBuilder builder;
    builder.SetShader(shader1);
    builder.SetInputTopology(EPrimitiveTopology::TriangleList);
    builder.SetPolygonMode(EPolygonMode::Fill);
    builder.DisableBlending();
    builder.DisableDepthTest();
    builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
    builder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
    builder.SetBufferLayout(bufferLayout);
    const auto pipeline = builder.Build(m_GraphicsContext.get());


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

    // Transition swapchain image to color attachment

    DrawGeometry(cmd);

    // Transition swapchain image to present
}

void Dissolve::DrawGeometry(const Ref<CommandBuffer>& cmd)
{
    //cmd->BeginRendering

    //pipeline->Bind(cmd);

    // Set viewport

    // Set scissors

    // cmd->Draw(3);

    // cmd->EndRendering();
}

Application* Elixir::CreateApplication()
{
    EE_PROFILE_ZONE_SCOPED()
    return new Dissolve();
}
