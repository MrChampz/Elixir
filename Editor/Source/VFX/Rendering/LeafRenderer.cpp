#include "LeafRenderer.h"
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <algorithm>
#include <utility>

namespace EditorVFX
{
    namespace
    {
        struct LeafPushConstants
        {
            uint32_t ParticleOffset, PointOffset, PointCount, BranchCount;
            uint32_t Interval;
            float Length, Width, MaturationSegments;
            glm::vec4 Color;
            uint32_t Shape;
        };
    }

    LeafRenderer::LeafRenderer(const Elixir::GraphicsContext* context, const Elixir::ShaderLoader* loader)
        : m_Context(context), m_FrameBuffers(*context)
    {
        m_Shader = loader->LoadShader("./EditorShaders", "Leaf");
        if (!m_Shader) { EE_CORE_ERROR("Could not load the Editor leaf shaders."); return; }
        Elixir::Aether::Rendering::SFrameData data{};
        m_FrameBuffers.ForEach([&](auto& buffer) { buffer = Elixir::UniformBuffer::Create(context, sizeof(data), &data); });
    }

    void LeafRenderer::Prepare(const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame,
        const Elixir::Camera& camera)
    {
        m_PendingFrame = frame;
        m_FrameData = {
            .View = camera.GetViewMatrix(), .Proj = camera.GetProjectionMatrix(),
            .ViewProj = camera.GetViewProjectionMatrix(), .CameraPos = camera.GetPosition(),
            .Time = frame ? frame->GetElapsedTimeSeconds() : 0,
        };
    }

    void LeafRenderer::Render(const Elixir::Ref<Elixir::Image>& color, const Elixir::Ref<Elixir::Image>& depth)
    {
        using namespace Elixir;
        const auto pending = std::exchange(m_PendingFrame, nullptr);
        if (!pending || !m_Shader || !color || !depth || std::none_of(m_Settings.begin(), m_Settings.end(),
            [](const auto& settings) { return settings && settings->Enabled; })) return;
        const auto& frame = *pending;
        const auto colorFormat = color->GetFormat();
        const auto depthFormat = EDepthStencilImageFormat(depth->GetFormat());
        if (!m_Pipeline || colorFormat != m_ColorFormat || depthFormat != m_DepthFormat)
        {
            PipelineBuilder builder;
            builder.SetShader(m_Shader);
            builder.SetInputTopology(EPrimitiveTopology::TriangleList);
            builder.SetPolygonMode(EPolygonMode::Fill);
            builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
            builder.SetColorAttachmentFormat(colorFormat);
            builder.SetDepthAttachmentFormat(depthFormat);
            builder.DisableBlending();
            auto info = builder.GetCreateInfo();
            info.DepthStencil.DepthTestEnable = true;
            info.DepthStencil.DepthWriteEnable = true;
            info.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;
            m_Pipeline = GraphicsPipeline::Create(m_Context, info);
            m_ColorFormat = colorFormat; m_DepthFormat = depthFormat;
        }
        if (!m_Pipeline) { EE_CORE_ERROR("Could not create the Editor leaf pipeline."); return; }
        auto& buffer = m_FrameBuffers.GetCurrent();
        if (!buffer || !buffer->IsValid()) return;
        buffer->UpdateData(&m_FrameData, sizeof(m_FrameData));
        m_Shader->BindConstantBuffer("cbFrame", buffer);
        const auto command = m_Context->GetSecondaryCommandBuffer();
        if (!command) return;
        const SRenderingInfo rendering{.ColorAttachment = color, .DepthStencilAttachment = depth, .DepthClearValue = std::nullopt, .RenderArea = color->GetExtent()};
        command->Begin(rendering);
        color->Barrier(command.get());
        depth->Barrier(command.get());
        command->BeginRendering(rendering);
        const auto extent = color->GetExtent();
        command->SetViewports({{.Width = float(extent.Width), .Height = float(extent.Height), .MaxDepth = 1}});
        command->SetScissors({{.Extent = {extent.Width, extent.Height}}});
        for (const auto& item : frame.GetItems())
        {
            if (item.RenderMode != Aether::Core::EParticleRenderMode::Strand || item.EmitterIndex >= m_Settings.size()) continue;
            const auto& settings = m_Settings[item.EmitterIndex];
            if (!settings || !settings->Enabled || !settings->GetLeafCount(item.Strand.PointCount)) continue;
            const auto resource = std::find_if(frame.GetResources().begin(), frame.GetResources().end(),
                [&item](const auto& candidate) { return candidate.Layout == item.ParticleStateLayout; });
            if (resource == frame.GetResources().end() || !resource->ParticleStateBuffer || !resource->StrandPointBuffer ||
                !resource->StrandPointBuffer->IsValid()) continue;
            const float scale = std::max({glm::length(glm::vec3(item.WorldTransform[0])),
                glm::length(glm::vec3(item.WorldTransform[1])), glm::length(glm::vec3(item.WorldTransform[2]))});
            LeafPushConstants constants{
                item.Allocation.Particles.Offset + item.LocalParticleOffset, item.StrandPointOffset,
                item.Strand.PointCount, item.Strand.BranchCount, settings->Interval,
                settings->Length * scale, settings->Width * scale, settings->MaturationSegments,
                {settings->Color[0],settings->Color[1],settings->Color[2],settings->Color[3]}, uint32_t(settings->Shape)
            };
            m_Shader->BindStorageBuffer("particles", resource->ParticleStateBuffer);
            m_Shader->BindStorageBuffer("strandPoints", resource->StrandPointBuffer);
            m_Shader->SetPushConstant("pc", &constants, sizeof(constants));
            m_Pipeline->Bind(command);
            command->Draw(settings->GetLeafCount(item.Strand.PointCount) * 444u, item.ParticleCount * (1 + item.Strand.BranchCount));
        }
        command->EndRendering();
        command->End();
        m_Context->EnqueueSecondaryCommandBuffer(command);
    }
}
