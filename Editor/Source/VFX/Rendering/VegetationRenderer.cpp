#include "VegetationRenderer.h"
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <algorithm>
#include <utility>
#include "../Vegetation/VegetationEffect.h"

namespace EditorVFX
{
    namespace
    {
        struct VegetationPushConstants
        {
            glm::mat4 WorldTransform;
            uint32_t ParticleOffset, ParticleCount, RadialSegments, Subdivisions;
            float WidthScale;
            uint32_t OrderStep, MaterialIndex = 0, MaxPoints;
            float RootWidthScale, TipWidthScale, WidthExponent, BranchWidthScale;
            uint32_t HasGrowth, BranchCount;
        };
    }

    VegetationRenderer::VegetationRenderer(
        const Elixir::GraphicsContext* context,
        const Elixir::ShaderLoader* loader
    )
        : m_Context(context), m_FrameBuffers(*context)
    {
        m_Shader = loader->LoadShader("./EditorShaders", "Vegetation");
        if (!m_Shader)
        {
            EE_CORE_ERROR("Could not load the Editor vegetation shaders.");
            return;
        }
        Elixir::Aether::Rendering::SFrameData data{};
        m_FrameBuffers.ForEach(
            [&](auto& buffer)
            {
                buffer = Elixir::UniformBuffer::Create(context, sizeof(data), &data);
            }
        );
    }

    void VegetationRenderer::Prepare(
        const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame,
        const Elixir::Camera& camera
    )
    {
        m_PendingFrame = frame;
        m_FrameData = {
            .View = camera.GetViewMatrix(),
            .Proj = camera.GetProjectionMatrix(),
            .ViewProj = camera.GetViewProjectionMatrix(),
            .CameraPos = camera.GetPosition(),
            .Time = frame ? frame->GetElapsedTimeSeconds() : 0,
        };
    }

    void VegetationRenderer::Render(
        const Elixir::Ref<Elixir::Image>& color,
        const Elixir::Ref<Elixir::Image>& depth
    )
    {
        using namespace Elixir;
        const auto pending = std::exchange(m_PendingFrame, nullptr);
        if (!pending || !m_Shader || !color || !depth)
            return;
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
            m_ColorFormat = colorFormat;
            m_DepthFormat = depthFormat;
        }
        if (!m_Pipeline)
        {
            EE_CORE_ERROR("Could not create the Editor vegetation pipeline.");
            return;
        }
        auto& buffer = m_FrameBuffers.GetCurrent();
        if (!buffer || !buffer->IsValid())
            return;
        buffer->UpdateData(&m_FrameData, sizeof(m_FrameData));
        m_Shader->BindConstantBuffer("cbFrame", buffer);
        const auto command = m_Context->GetSecondaryCommandBuffer();
        if (!command)
            return;
        const SRenderingInfo rendering{
            .ColorAttachment = color,
            .DepthStencilAttachment = depth,
            .DepthClearValue = std::nullopt,
            .RenderArea = color->GetExtent()
        };
        command->Begin(rendering);
        color->Barrier(command.get());
        depth->Barrier(command.get());
        command->BeginRendering(rendering);
        const auto extent = color->GetExtent();
        command->SetViewports(
            {{.Width = float(extent.Width), .Height = float(extent.Height), .MaxDepth = 1}}
        );
        command->SetScissors({{.Extent = {extent.Width, extent.Height}}});
        for (const auto& item : frame.GetItems())
        {
            if (item.RenderMode != Aether::Core::EParticleRenderMode::None)
                continue;
            const auto* config =
                dynamic_cast<const VegetationRenderConfig*>(item.RenderConfig.get());
            if (!config || !config->IsValid() ||
                uint64_t(config->MaxPoints) * config->OrderStep > item.ParticleCount)
                continue;
            const auto resource = std::find_if(
                frame.GetResources().begin(),
                frame.GetResources().end(),
                [&item](const auto& candidate)
                {
                    return candidate.Layout == item.ParticleStateLayout;
                }
            );
            if (resource == frame.GetResources().end() || !resource->ParticleStateBuffer ||
                !resource->ParticleStateBuffer->IsValid())
                continue;
            const auto& constraint = config->Constraint;
            VegetationPushConstants constants{
                .WorldTransform = item.WorldTransform,
                .ParticleOffset = item.Allocation.Particles.Offset + item.LocalParticleOffset,
                .ParticleCount = item.ParticleCount,
                .RadialSegments = config->RadialSegments,
                .Subdivisions = config->Subdivisions,
                .WidthScale = config->WidthScale,
                .OrderStep = config->OrderStep,
                .MaxPoints = config->MaxPoints,
                .RootWidthScale = config->RootWidthScale,
                .TipWidthScale = config->TipWidthScale,
                .WidthExponent = config->WidthExponent,
                .BranchWidthScale = config->BranchWidthScale,
                .HasGrowth = constraint.HasGrowth ? 1u : 0u,
                .BranchCount = constraint.HasGrowth ? constraint.Growth.BranchCount : 0u,
            };
            m_Shader->BindStorageBuffer("particles", resource->ParticleStateBuffer);
            m_Shader->SetPushConstant("pc", &constants, sizeof(constants));
            m_Pipeline->Bind(command);
            command->Draw(config->GetSegmentVertexCount(), item.ParticleCount);
        }
        command->EndRendering();
        command->End();
        m_Context->EnqueueSecondaryCommandBuffer(command);
    }
}
