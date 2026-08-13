#include "GameViewRenderer.h"

#include <Engine/Graphics/Pipeline/PipelineBuilder.h>

using namespace Elixir;

GameViewRenderer::GameViewRenderer(
    const GraphicsContext* context,
    const ShaderLoader* shaderLoader
) : m_GraphicsContext(context)
{
    m_RenderTarget = CreateRenderTarget();

    m_Shader = shaderLoader->LoadShader("./Shaders/", "EditorGameView");
    m_FrameConstantBuffer = UniformBuffer::Create(m_GraphicsContext, sizeof(SFrameData));
    m_Shader->BindConstantBuffer("cbGameViewFrame", m_FrameConstantBuffer);

    PipelineBuilder builder;
    builder.SetShader(m_Shader);
    builder.SetInputTopology(EPrimitiveTopology::TriangleList);
    builder.SetPolygonMode(EPolygonMode::Fill);
    builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
    builder.DisableBlending();
    builder.DisableDepthTest();
    builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
    builder.SetBufferLayout({});
    m_Pipeline = builder.Build(context);
}

Ref<Texture2D> GameViewRenderer::CreateRenderTarget() const
{
    auto targetInfo = Texture2D::CreateImageInfo(
        EImageFormat::R8G8B8A8_SRGB,
        m_Extent.Width,
        m_Extent.Height
    );
    targetInfo.Usage = EImageUsage::ColorAttachment |
        EImageUsage::Sampled |
        EImageUsage::TransferSrc;
    targetInfo.InitialLayout = EImageLayout::ShaderReadOnly;
    return Texture2D::Create(m_GraphicsContext, targetInfo, "EditorGameRenderTarget");
}

bool GameViewRenderer::Resize(const Extent2D& extent)
{
    if (extent.Width == 0 || extent.Height == 0)
        return false;
    if (extent.Width == m_Extent.Width && extent.Height == m_Extent.Height)
        return false;

    // The previous target may still be sampled by an in-flight UI frame.
    m_GraphicsContext->WaitDeviceIdle();
    m_Extent = extent;
    m_RenderTarget = CreateRenderTarget();
    EE_CORE_TRACE(
        "Scene render target resized: [Width = {}, Height = {}, AspectRatio = {:.4f}].",
        m_Extent.Width,
        m_Extent.Height,
        static_cast<float>(m_Extent.Width) / static_cast<float>(m_Extent.Height)
    )
    return true;
}

void GameViewRenderer::Render() const
{
    const SFrameData frameData {
        .Viewport = {
            static_cast<float>(m_Extent.Width),
            static_cast<float>(m_Extent.Height),
        },
        .AspectRatio = static_cast<float>(m_Extent.Width) /
            static_cast<float>(m_Extent.Height),
    };
    m_FrameConstantBuffer->UpdateData(&frameData, sizeof(frameData));

    const SRenderingInfo renderingInfo {
        .ColorAttachment = m_RenderTarget,
        .RenderArea = m_Extent,
    };

    const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
    cmd->Begin(renderingInfo);
    m_RenderTarget->Transition(cmd, EImageLayout::ColorAttachment);
    cmd->BeginRendering(renderingInfo);
    cmd->SetViewports({ {
        .X = 0.0f,
        .Y = 0.0f,
        .Width = static_cast<float>(m_Extent.Width),
        .Height = static_cast<float>(m_Extent.Height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    } });
    cmd->SetScissors({ {
        .Offset = { 0, 0 },
        .Extent = m_Extent,
    } });
    m_Pipeline->Bind(cmd);
    cmd->Draw(6);
    cmd->EndRendering();
    m_RenderTarget->Transition(cmd, EImageLayout::ShaderReadOnly);
    m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
}
