#include "epch.h"
#include "Renderer.h"

#include <Engine/Core/Color.h>
#include <Engine/GUI/Renderer/QuadRenderPass.h>
#include <Engine/GUI/Renderer/TextRenderPass.h>
#include <Engine/GUI/Renderer/DebugRenderPass.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/CommandBuffer.h>

namespace Elixir::GUI
{
    Renderer::Renderer(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const Extent2D& extent
    ) : m_RenderExtent(extent), m_GraphicsContext(context)
    {
        EE_CORE_ASSERT(extent.Width > 0 && extent.Height > 0, "Render extent must be greater than zero!")
        EE_CORE_INFO("Initializing GUI Renderer [Width={}, Height={}].", extent.Width, extent.Height)
        InitPerFrameData();
        InitRenderPasses(shaderLoader);
    }

    void Renderer::Resize(const Extent2D& extent)
    {
        EE_CORE_ASSERT(extent.Width > 0 && extent.Height > 0, "Render extent must be greater than zero!")
        EE_CORE_INFO("Resizing GUI Renderer [Width={}, Height={}].", extent.Width, extent.Height)

        m_RenderExtent = extent;
        m_PerFrameData.RenderExtent = { (float)extent.Width, (float)extent.Height };
        m_PerFrameConstantBuffer->UpdateData(&m_PerFrameData, sizeof(SPerFrameData));
    }

    void Renderer::Render(const RenderBatch& batch) const
    {
        for (const auto& pass : m_RenderPasses)
        {
            pass->GenerateDrawCommands(batch);
        }

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        BeginRendering(cmd);

        for (const auto& pass : m_RenderPasses)
        {
            if (pass->HasData())
            {
                pass->Render(cmd);
            }
        }

        EndRendering(cmd);
    }

    void Renderer::RegisterRenderPass(const Ref<RenderPass>& pass)
    {
        m_RenderPasses.push_back(pass);
        EE_CORE_TRACE("GUI: Registered RenderPass.")
    }

    void Renderer::InitPerFrameData()
    {
        m_PerFrameData.RenderExtent = {
            (float)m_RenderExtent.Width,
            (float)m_RenderExtent.Height
        };

        m_PerFrameConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SPerFrameData),
            &m_PerFrameData
        );
    }

    void Renderer::InitRenderPasses(const ShaderLoader* shaderLoader)
    {
        const auto& quad = CreateRef<QuadRenderPass>(
            m_GraphicsContext,
            shaderLoader,
            m_PerFrameConstantBuffer
        );
        RegisterRenderPass(quad);

        const auto& text = CreateRef<TextRenderPass>(
            m_GraphicsContext,
            shaderLoader,
            m_PerFrameConstantBuffer
        );
        RegisterRenderPass(text);

        const auto& debug = CreateRef<DebugRenderPass>(
            m_GraphicsContext,
            shaderLoader,
            m_PerFrameConstantBuffer
        );
        RegisterRenderPass(debug);
    }

    void Renderer::BeginRendering(const Ref<CommandBuffer>& cmd) const
    {
        const auto renderingInfo = SRenderingInfo
        {
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .RenderArea = m_RenderExtent
        };

        Viewport viewport = {};
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = m_RenderExtent.Width;
        viewport.Height = m_RenderExtent.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        Rect2D scissor = {};
        scissor.Offset = { 0, 0 };
        scissor.Extent = m_RenderExtent;

        cmd->BeginRendering(renderingInfo);
        cmd->SetViewports({ viewport });
        cmd->SetScissors({ scissor });
    }

    void Renderer::EndRendering(const Ref<CommandBuffer>& cmd) const
    {
        cmd->EndRendering();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
    }
}