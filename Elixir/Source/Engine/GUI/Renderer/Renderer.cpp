#include "epch.h"
#include "Renderer.h"

#include "glm/gtc/matrix_transform.hpp"

#include <Engine/Core/Color.h>
#include <Engine/GUI/Widget.h>
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
    ) : m_DPIScale(context->GetDPIScale()), m_RenderExtent(extent), m_GraphicsContext(context)
    {
        EE_CORE_ASSERT(extent.Width > 0 && extent.Height > 0, "Render extent must be greater than zero!")
        EE_CORE_INFO("Initializing GUI Renderer {}.", extent)

        InitPerFrameData();
        InitRenderPasses(shaderLoader);
    }

    void Renderer::Resize(const Extent2D& extent)
    {
        EE_CORE_ASSERT(extent.Width > 0 && extent.Height > 0, "Render extent must be greater than zero!")
        EE_CORE_INFO("Resizing GUI Renderer {}.", extent)

        m_RenderExtent = extent;
        CalculateProjectionMatrix();
        m_PerFrameConstantBuffer->UpdateData(&m_PerFrameData, sizeof(SPerFrameData));
    }

    void Renderer::Rebuild(const RenderBatch& batch)
    {
        for (const auto& pass : m_RenderPasses)
            pass->BeginFrame();

        m_DrawItems.clear();

        for (const auto& run : batch.GetRuns())
        {
            const auto it = m_PassesByType.find(run.Type);
            if (it == m_PassesByType.end()) continue;

            const auto pass = it->second;
            const std::span range(batch.GetCommands().data() + run.First, run.Count);

            const auto firstInstance = pass->AppendRange(range);
            const auto instanceCount = pass->GetInstanceCount() - firstInstance;

            if (instanceCount > 0)
                m_DrawItems.push_back({ pass, firstInstance, instanceCount });
        }

        for (const auto& pass : m_RenderPasses)
            pass->EndFrame();
    }

    void Renderer::Draw() const
    {
        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        BeginRendering(cmd);

        RenderPass* lastPass = nullptr;
        for (const auto& item : m_DrawItems)
        {
            if (item.Pass != lastPass)
            {
                item.Pass->Bind(cmd);
                lastPass = item.Pass;
            }

            item.Pass->Render(cmd, item.FirstInstance, item.InstanceCount);
        }

        EndRendering(cmd);
    }

    void Renderer::RegisterRenderPass(const Ref<RenderPass>& pass)
    {
        m_RenderPasses.push_back(pass);
        m_PassesByType[pass->GetHandleType()] = pass.get();
        EE_CORE_TRACE("GUI: Registered RenderPass.")
    }

    void Renderer::InitPerFrameData()
    {
        CalculateProjectionMatrix();

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
            m_DPIScale,
            m_PerFrameConstantBuffer
        );
        RegisterRenderPass(quad);

        const auto& text = CreateRef<TextRenderPass>(
            m_GraphicsContext,
            shaderLoader,
            m_DPIScale,
            m_PerFrameConstantBuffer
        );
        RegisterRenderPass(text);

        const auto& debug = CreateRef<DebugRenderPass>(
            m_GraphicsContext,
            shaderLoader,
            m_DPIScale,
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

    void Renderer::CalculateProjectionMatrix()
    {
        // Orthographic projection
        m_PerFrameData.Proj = glm::ortho(0.0f, (float)m_RenderExtent.Width, 0.0f, (float)m_RenderExtent.Height);
    }
}