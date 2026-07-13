#include "epch.h"
#include "PostProcessor.h"

#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/SamplerBuilder.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir
{
    PostProcessor::PostProcessor(const GraphicsContext* context, const ShaderLoader* shaderLoader)
        : m_Context(context)
    {
        EE_CORE_INFO("Initializing Post Processor.")

        m_Shader = shaderLoader->LoadShader("./Shaders/", "PostProcess");

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.DisableBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout({});
        m_Pipeline = builder.Build(context);

        m_Buffer = UniformBuffer::Create(context, sizeof(SPostData), &m_Data);
        m_Sampler = SamplerBuilder().Build(context);

        m_Shader->BindConstantBuffer("cbPost", m_Buffer);
        m_Shader->BindSampler("postSampler", m_Sampler);
    }

    void PostProcessor::Apply()
    {
        const auto rt = m_Context->GetRenderTarget();
        const auto extent = rt->GetExtent();

        // (Re)create the grab target to match the render target.
        if (!m_SceneColor || m_Extent.Width != extent.Width || m_Extent.Height != extent.Height)
        {
            m_SceneColor = Texture2D::Create(
                m_Context, EImageFormat::R8G8B8A8_SRGB, extent.Width, extent.Height, nullptr);
            m_Shader->BindTexture("sceneTexture", m_SceneColor);
            m_Extent = { extent.Width, extent.Height, 1 };
        }

        m_Data.ScreenSize = { (float)extent.Width, (float)extent.Height };
        m_Buffer->UpdateData(&m_Data, sizeof(SPostData));

        const auto cmd = m_Context->GetSecondaryCommandBuffer();

        const SRenderingInfo renderingInfo{
            .ColorAttachment = rt,
            .RenderArea = { extent.Width, extent.Height }
        };

        // Open the command buffer before the grab: the scene copy happens outside a
        // rendering scope, so we can't rely on BeginRendering to start recording.
        cmd->Begin(renderingInfo);

        // Grab the rendered scene, then re-draw it through the post shader back into
        // the render target.
        m_Context->BlitToTexture(cmd, m_SceneColor);

        cmd->BeginRendering(renderingInfo);

        Viewport viewport{};
        viewport.Width = (float)extent.Width;
        viewport.Height = (float)extent.Height;
        viewport.MaxDepth = 1.0f;

        Rect2D scissor{};
        scissor.Extent = { extent.Width, extent.Height };

        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });

        m_Pipeline->Bind(cmd);
        cmd->Draw(3);

        cmd->EndRendering();
        m_Context->EnqueueSecondaryCommandBuffer(cmd);
    }
}
