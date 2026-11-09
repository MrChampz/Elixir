#include <epch.h>
#include "VegetationProcessor.h"
#include "VegetationData.h"
#include <Engine/Aether/Emitter.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>

namespace EditorVFX
{
    using namespace Elixir;
    using namespace Elixir::Aether::Simulation;
    namespace
    {
        struct VegetationPushConstants
        {
            uint32_t ParticleOffset, ParticleCount, MaxPoints, OrderStep;
            float SegmentLength, Stiffness;
            uint32_t SolverIterations, PinFirstParticle;
            float DeltaSeconds;
        };

        class VegetationRuntime final : public ParticleProcessorRuntime
        {
          public:
            /**
             * @brief Creates the sample compute pipeline and its fixed uniform descriptor.
             * @param context Graphics context owning the resources.
             * @param loader Loader used during initialization.
             * @param settings Immutable vegetation settings.
             * @param playback Editor-owned playback control.
             */
            VegetationRuntime(
                const GraphicsContext* context,
                const ShaderLoader& loader,
                VegetationSettings settings,
                Ref<VegetationPlayback> playback
            )
                : m_Context(context), m_Settings(settings), m_Playback(std::move(playback))
            {
                m_Shader = loader.LoadShader(
                    "./EditorShaders",
                    "VegetationConstraints",
                    EShaderStage::Compute
                );
                if (!m_Shader)
                    return;
                m_Constants = UniformBuffer::Create(context, sizeof(SConstraintFrameData));
                if (!m_Constants || !m_Constants->IsValid())
                    return;
                m_Shader->BindConstantBuffer("cbConstraint", m_Constants);
                m_Pipeline = ComputePipeline::Create(context, {.Shader = m_Shader});
            }

            /** @brief Reports whether initialization succeeded. */
            bool IsReady() const { return m_Pipeline && m_Shader && m_Constants; }

            /**
             * @brief Records sample development and its constraint solver.
             * @param context Current emitter allocation and simulation step.
             */
            bool Record(const SParticleProcessingContext& context) override
            {
                if (!IsReady() ||
                    context.Layout != Aether::Core::EParticleStateLayout::CoreV1 ||
                    !context.Particles || !context.Command)
                    return false;
                const auto frame = m_Context->GetFrameNumber();
                auto& uploads = m_Uploads[m_Context->GetFrameIndex()];
                if (m_LastFrame != frame)
                {
                    uploads.clear();
                    m_LastFrame = frame;
                }
                auto data = BuildConstraintFrameData(m_Settings, context.WorldTransform);
                if (!data)
                    return false;
                if (m_Playback && m_Playback->Paused.load() && data->Topology.w == 0)
                    data->Topology.w = uint32_t(EStrandGrowthState::Paused);
                const auto upload = StagingBuffer::Create(m_Context, sizeof(*data), &*data);
                if (!upload || !upload->IsValid())
                    return false;
                uploads.push_back(upload);
                m_Constants->Barrier(
                    context.Command,
                    EPipelineStage::Transfer,
                    EPipelineAccess::TransferWrite
                );
                context.Command->CopyBuffer(upload, m_Constants);
                m_Constants->Barrier(
                    context.Command,
                    EPipelineStage::ComputeShader,
                    EPipelineAccess::UniformRead
                );
                m_Shader->BindStorageBuffer("particles", context.Particles);
                const VegetationPushConstants constants{
                    context.ParticleOffset,
                    context.ParticleCount,
                    m_Settings.MaxPoints,
                    m_Settings.OrderStep,
                    m_Settings.SegmentLength,
                    m_Settings.Stiffness,
                    m_Settings.SolverIterations,
                    m_Settings.PinFirstParticle ? 1u : 0u,
                    context.DeltaSeconds
                };
                m_Pipeline->Bind(context.Command);
                m_Shader->SetPushConstant(
                    context.Command,
                    "pc",
                    (void*)&constants,
                    sizeof(constants)
                );
                context.Command->Dispatch((context.ParticleCount + 255) / 256);
                return true;
            }

          private:
            const GraphicsContext* m_Context;
            VegetationSettings m_Settings;
            Ref<VegetationPlayback> m_Playback;
            Ref<Shader> m_Shader;
            Ref<ComputePipeline> m_Pipeline;
            Ref<UniformBuffer> m_Constants;
            uint32_t m_LastFrame = UINT32_MAX;
            std::array<std::vector<Ref<StagingBuffer>>, GraphicsContext::FRAMES> m_Uploads;
        };
    }

    VegetationProcessor::VegetationProcessor(
        VegetationSettings settings,
        Ref<VegetationPlayback> playback
    )
        : m_Settings(settings), m_Playback(std::move(playback))
    {
    }

    bool VegetationProcessor::IsCompatible(const Aether::SCompiledEmitter& emitter) const
    {
        return m_Settings.IsValid() && emitter.ConstraintCount == 0 &&
               uint64_t(m_Settings.MaxPoints) * m_Settings.OrderStep == emitter.MaxParticles &&
               !emitter.KillOnLifetimeExpired && emitter.SpawnRatePerSecond == 0 &&
               emitter.BurstCount == emitter.MaxParticles && emitter.BurstIntervalSeconds == 0;
    }

    Scope<ParticleProcessorRuntime> VegetationProcessor::CreateRuntime(
        const GraphicsContext* context,
        const ShaderLoader& loader
    ) const
    {
        auto runtime = CreateScope<VegetationRuntime>(context, loader, m_Settings, m_Playback);
        if (!runtime->IsReady())
            return nullptr;
        return runtime;
    }
}
