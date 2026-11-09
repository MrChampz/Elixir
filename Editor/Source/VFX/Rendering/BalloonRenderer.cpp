#include "BalloonRenderer.h"
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <algorithm>
#include <utility>
#include "../Vegetation/VegetationEffect.h"

namespace EditorVFX
{
    namespace
    {
        struct BalloonPushConstants
        {
            glm::mat4 WorldTransform;
            uint32_t ParticleOffset, ParticleCount, PointCount, OrderStep;
            float Radius, Height, Tilt, ColorVariation;
            glm::vec4 Color;
            uint32_t Seed, HasGrowth, BranchCount, Padding = 0;
        };
    }

    BalloonRenderer::BalloonRenderer(
        const Elixir::GraphicsContext* context,
        const Elixir::ShaderLoader* loader
    )
        : m_Context(context), m_FrameBuffers(*context)
    {
        m_Shader = loader->LoadShader("./EditorShaders", "Balloon");
        if (!m_Shader)
        {
            EE_CORE_ERROR("Could not load the Editor balloon shaders.");
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

    void BalloonRenderer::Prepare(
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

    void BalloonRenderer::Render(
        const Elixir::Ref<Elixir::Image>& color,
        const Elixir::Ref<Elixir::Image>& depth
    )
    {
        using namespace Elixir;
        const auto pending = std::exchange(m_PendingFrame, nullptr);
        const bool hasSettings = std::any_of(
            m_Settings.begin(),
            m_Settings.end(),
            [](const auto& settings)
            {
                return settings && settings->Enabled;
            }
        );
        if (!pending || !m_Shader || !color || !depth || !hasSettings)
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
            EE_CORE_ERROR("Could not create the Editor balloon pipeline.");
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
            if (item.RenderMode != Aether::Core::EParticleRenderMode::None ||
                item.EmitterIndex >= m_Settings.size())
                continue;
            const auto& settings = m_Settings[item.EmitterIndex];
            const auto* config = dynamic_cast<const VegetationRenderConfig*>(
                item.RenderConfig.get()
            );
            if (!settings || !settings->Enabled || !settings->IsValid() || !config ||
                config->MaxPoints < 2 || config->MaxPoints > 96 ||
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
            const bool hasGrowth = config->Constraint.HasGrowth;
            const uint32_t branches = hasGrowth ? config->Constraint.Growth.BranchCount : 0;
            BalloonPushConstants constants{
                item.WorldTransform,
                item.Allocation.Particles.Offset + item.LocalParticleOffset,
                item.ParticleCount,
                config->MaxPoints,
                config->OrderStep,
                settings->Radius,
                settings->Height,
                settings->Tilt,
                settings->ColorVariation,
                {settings->Color[0],
                 settings->Color[1],
                 settings->Color[2],
                 settings->Color[3]},
                settings->Seed,
                hasGrowth ? 1u : 0u,
                branches
            };
            m_Shader->BindStorageBuffer("particles", resource->ParticleStateBuffer);
            m_Shader->SetPushConstant("pc", &constants, sizeof(constants));
            m_Pipeline->Bind(command);
            command->Draw(2352u, config->OrderStep / (1u + branches));
        }
        command->EndRendering();
        command->End();
        m_Context->EnqueueSecondaryCommandBuffer(command);
    }
}
