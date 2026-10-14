#include "epch.h"
#include "Simulator.h"

#include <Engine/Core/Timer.h>
#include <cmath>
#include <Engine/Aether/Core/Operation.h>

namespace Elixir::Aether::Simulation
{
    using namespace Core;
    using namespace Modules;
    using Rendering::SystemInstanceRenderProxy;
    using Rendering::FrameSubmission;

    namespace
    {
        struct alignas(16) SParamsData
        {
            glm::vec4 Time{};
            glm::vec4 Viewport{};
        };

        struct alignas(16) SGPUEmitter
        {
            uint32_t    LocalParticleOffset{};
            uint32_t    MaxParticles{};
            uint32_t    SpawnOperationOffset{};
            uint32_t    SpawnOperationCount{};
            uint32_t    UpdateOperationOffset{};
            uint32_t    UpdateOperationCount{};
            uint32_t    TriggerTargetOffset{};
            uint32_t    TriggerTargetCount{};
            uint32_t    RenderMode{};
            float       SpawnRatePerSecond{};
            float       GravityScale{};
            float       BurstIntervalSeconds{};
            uint32_t    BurstCount{};
            uint32_t    IsTriggerDriven{};
            uint32_t    KillOnLifetimeExpired = 1;
            uint32_t    Padding = 0;
            uint32_t    StrandUpdateOperationOffset = 0;
            uint32_t    StrandUpdateOperationCount = 0;
            uint32_t    Padding2 = 0;
            uint32_t    Padding3 = 0;
        };

        struct SGPUSystemInstance
        {
            uint32_t ParticleBaseOffset = 0;
            uint32_t EmitterBaseOffset = 0;
            uint32_t OperationBaseOffset = 0;
            uint32_t OperandBaseOffset = 0;
            uint32_t ConstantBaseOffset = 0;
            uint32_t ParameterBaseOffset = 0;

            uint32_t EmitterStateBaseOffset = 0;
            uint32_t SpawnRequestBaseOffset = 0;
            uint32_t TriggerEventBaseOffset = 0;
            uint32_t TriggerQueueStateBaseOffset = 0;

            uint32_t ParticleCount = 0;
            uint32_t EmitterCount = 0;
            uint32_t TriggerEventCapacityPerEmitter = 0;
            uint32_t Generation = 0;

            uint32_t ParticleStateLayoutIndex = 0;
        };

        struct SGPUEmitterInstanceState
        {
            float SpawnAccumulator = 0.0f;
            float BurstAccumulator = 0.0f;
            uint32_t BufferCursor = 0;
            uint32_t EmissionIndex = 0;

            uint32_t Generation = 0;
        };

        struct SGPUSpawnRequest
        {
            uint32_t SpawnCursor = 0;
            uint32_t SpawnCount = 0;
            uint32_t EmissionIndex = 0;
            uint32_t Generation = 0;
        };

        struct SGPUTriggerTarget
        {
            uint32_t TargetEmitterIndex = 0;
            uint32_t BurstCount = 0;
            float DelaySeconds = 0.0f;
        };

        struct SGPUTriggerEvent
        {
            float RemainingDelaySeconds = 0.0f;
            uint32_t SpawnCount = 0;
            uint32_t Generation = 0;
        };

        struct SGPUTriggerQueueState
        {
            uint32_t Count = 0;
            uint32_t OverflowCount = 0;
        };

        struct SGPUSystemSchedulerState
        {
            uint32_t Generation = 0;
            uint32_t ActiveTriggerBufferIndex = 0;
            uint32_t ResetPending = 0;
        };

        // Mirrors one CoreV1 particle state in GPU storage.
        struct alignas(16) SGPUParticle
        {
            glm::vec3   Position{};
            float       Size{};
            glm::vec3   Velocity{};
            float       Age{};
            glm::vec4   Transform{};
            glm::vec3   Tangent{};
            uint32_t    RibbonId = 0;
            glm::vec4   Color{};
            uint32_t    EmitterIndex = 0;
            uint32_t    LinkOrder = 0;
            float       Lifetime{};
            float       Alive{};
        };

        // Identifies the system instance processed by scheduler compute dispatches.
        struct SSchedulePushConstants
        {
            uint32_t InstanceIndex = 0;
        };

        // Identifies the system instance and emitter processed by spawn dispatches.
        struct SSpawnPushConstants
        {
            uint32_t InstanceIndex = 0;
            uint32_t EmitterIndex = 0;
        };

        // Matches the strand compute push-constant layout (108 bytes).
        struct SStrandPushConstants
        {
            glm::mat4 RootTransform{1.0f};
            uint32_t ParticleOffset = 0;
            uint32_t PointOffset = 0;
            uint32_t PointCount = 0;
            uint32_t StrandCount = 0;
            float SegmentLength = 0;
            float Stiffness = 0;
            float Damping = 0;

            uint32_t Generation = 0;
            uint32_t SolverIterations = 0;
            uint32_t InstanceIndex = 0;
            uint32_t EmitterIndex = 0;
        };

        // Update dispatches use the same ABI as scheduler dispatches.
        using SUpdatePushConstants = SSchedulePushConstants;

        SGPUEmitter ToEmitterData(
            const SCompiledEmitter& emitter,
            uint32_t operationBaseOffset,
            uint32_t triggerTargetBaseOffset
        )
        {
            return {
                .LocalParticleOffset = emitter.LocalParticleOffset,
                .MaxParticles = emitter.MaxParticles,
                .SpawnOperationOffset = operationBaseOffset + emitter.SpawnOperationOffset,
                .SpawnOperationCount = emitter.SpawnOperationCount,
                .UpdateOperationOffset = operationBaseOffset + emitter.UpdateOperationOffset,
                .UpdateOperationCount = emitter.UpdateOperationCount,
                .TriggerTargetOffset = triggerTargetBaseOffset + emitter.TriggerTargetOffset,
                .TriggerTargetCount = emitter.TriggerTargetCount,
                .RenderMode = uint32_t(emitter.RenderMode),
                .SpawnRatePerSecond = emitter.SpawnRatePerSecond,
                .GravityScale = emitter.GravityScale,
                .BurstIntervalSeconds = emitter.BurstIntervalSeconds,
                .BurstCount = emitter.BurstCount,
                .IsTriggerDriven = emitter.IsTriggerDriven ? 1u : 0u,
                .KillOnLifetimeExpired = emitter.KillOnLifetimeExpired ? 1u : 0u,
                .StrandUpdateOperationOffset = operationBaseOffset + emitter.StrandUpdateOperationOffset,
                .StrandUpdateOperationCount = emitter.StrandUpdateOperationCount,
            };
        }

        SGPUTriggerTarget ToTriggerTargetData(const SCompiledTriggerTarget& target)
        {
            return {
                .TargetEmitterIndex = target.TargetEmitterIndex,
                .BurstCount = target.BurstCount,
                .DelaySeconds = target.DelaySeconds,
            };
        }

        glm::mat4 GetParticleRenderTransform(
            const SCompiledEmitter& emitter,
            const SystemInstanceRenderProxy& proxy
        )
        {
            return emitter.SimulationSpace == EParticleSimulationSpace::Local
                ? proxy.GetWorldTransform()
                : glm::mat4{ 1.0f };
        }
    }

    Simulator::Simulator(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const SResourcePoolLimits& limits
    ) : m_Limits(limits),
        m_ParticleStateLayouts(m_Limits.ParticleCapacity),
        m_ResourcePool(m_Limits, m_ParticleStateLayouts),
        m_GraphicsContext(context)
    {
        static_assert(sizeof(SGPUParticle) == PARTICLE_STATE_CORE_V1_STRIDE);
        EE_CORE_ASSERT(context, "Aether Simulator requires a graphics context.")
        EE_CORE_ASSERT(shaderLoader, "Aether Simulator requires a shader loader.")
        EE_CORE_INFO("Initializing Aether Simulator.")

        if (!context || !shaderLoader ||
            m_Limits.StrandPointCapacity > INT32_MAX / (3 * sizeof(glm::vec4)) ||
            m_Limits.ConstantCapacity > UINT32_MAX / sizeof(uint32_t) ||
            m_Limits.ParameterWordCapacity > UINT32_MAX / sizeof(uint32_t))
        {
            EE_CORE_ERROR("Aether requires a graphics context, a shader loader, and value buffers within the 32-bit byte address range.");
            return;
        }

        Init(shaderLoader);
        CreateBuffers();
        if (!m_StrandPointBuffer || !m_StrandPointBuffer->IsValid()) return;
        BindShaderParameters();
    }

    void Simulator::BeginFrame(const Timestep& timestep)
    {
        if (!m_ConstantBuffer || !m_ParameterBuffer) return;
        ProcessCompletedRetirements();
        m_ParameterUploads[m_GraphicsContext->GetFrameIndex()].clear();
        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    Ref<const RenderFrame> Simulator::Simulate(
        const FrameSubmission& submission,
        const Ref<CommandBuffer>& cmd
    )
    {
        return RecordSteps(submission, cmd, std::span(&m_LastDeltaTimeSeconds, 1),
            m_ElapsedTimeSeconds - m_LastDeltaTimeSeconds);
    }

    Ref<const RenderFrame> Simulator::RecordSteps(
        const FrameSubmission& submission,
        const Ref<CommandBuffer>& cmd,
        const std::span<const float> steps,
        const float initialTime
    )
    {
        EE_CORE_ASSERT(cmd, "Aether simulation requires a command buffer.")

        if (!cmd || !m_ConstantBuffer || !m_ParameterBuffer ||
            !m_StrandPointBuffer || !m_StrandPointBuffer->IsValid() || steps.empty())
            return nullptr;

        const auto invalidStep = std::ranges::any_of(steps, [](const float step) {
            return !std::isfinite(step) || step < 0;
        });

        if (!std::isfinite(initialTime) || initialTime < 0 || invalidStep)
        {
            EE_CORE_ERROR("Aether simulation requires finite, nonnegative step and initial times.");
            return nullptr;
        }

        const auto serial = ++m_SubmissionSerial;
        m_LastMetrics = {
            .SubmissionSerial = serial,
            .DeltaTimeSeconds = steps.back(),
            .ElapsedTimeSeconds = initialTime,
            .RequestedSystemInstanceCount = submission.GetRenderProxies().size(),
            .TriggerEventCapacityPerEmitter = m_Limits.TriggerEventCapacityPerEmitter,
        };

        const auto instances = ResolveSubmittedInstances(submission);
        const auto batches = BuildSimulationBatches(instances);
        const auto extent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        std::vector<SParamsData> parameters;
        parameters.reserve(steps.size());

        float elapsed = initialTime;
        for (const float step : steps)
        {
            elapsed += step;
            parameters.push_back({
                .Time = { step, elapsed, 0, 0 },
                .Viewport = { (float)extent.Width, (float)extent.Height, 0, 0 }
            });
        }

        const auto upload = StagingBuffer::Create(
            m_GraphicsContext,
            parameters.size() * sizeof(SParamsData),
            parameters.data()
        );
        if (!upload || !upload->IsValid())
        {
            EE_CORE_ERROR("Could not allocate Aether simulation time upload.");
            return nullptr;
        }

        m_ParameterUploads[m_GraphicsContext->GetFrameIndex()].push_back(upload);

        for (size_t index = 0; index < steps.size(); ++index)
        {
            m_ParamsBuffer->Barrier(
                cmd,
                EPipelineStage::Transfer,
                EPipelineAccess::TransferWrite
            );

            SBufferCopy region{
                .SrcOffset = index * sizeof(SParamsData),
                .DstOffset = 0,
                .Size = sizeof(SParamsData)
            };

            cmd->CopyBuffer(
                upload,
                m_ParamsBuffer,
                std::span(&region, 1)
            );

            m_ParamsBuffer->Barrier(
                cmd,
                EPipelineStage::ComputeShader,
                EPipelineAccess::UniformRead
            );

            for (const auto& batch : batches)
                SimulateBatch(cmd, batch);
        }

        for (const auto& batch : batches)
            PublishRenderBarrier(cmd, batch.ParticleStateLayout);

        m_LastDeltaTimeSeconds = steps.back();
        m_ElapsedTimeSeconds = elapsed;

        m_LastMetrics.DeltaTimeSeconds = m_LastDeltaTimeSeconds;
        m_LastMetrics.ElapsedTimeSeconds = elapsed;
        m_LastMetrics.SimulationBatchCount = batches.size();

        return CreateRef<RenderFrame>(
            BuildRenderResources(),
            m_EmitterBuffer,
            BuildRenderItems(instances),
            serial,
            m_ElapsedTimeSeconds
        );
    }

    void Simulator::Retire(const SSystemInstanceKey& key)
    {
        const auto found = m_InstanceRecords.find(key);
        if (found == m_InstanceRecords.end())
            return;

        QueueRetirement(found->second.Allocation);
        m_InstanceRecords.erase(found);
        m_AllocationFailures.erase(key);
        m_UnsupportedParticleStateLayoutInstances.erase(key);
    }

    void Simulator::Init(const ShaderLoader* shaderLoader)
    {
        m_SchedulerBeginShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSchedulerBegin" },
            "ParticlesSchedulerBegin",
            EShaderStage::Compute
        );

        m_SchedulerInitEmittersShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSchedulerInitEmitters" },
            "ParticlesSchedulerInitEmitters",
            EShaderStage::Compute
        );

        m_SchedulerScheduleEmittersShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSchedulerScheduleEmitters" },
            "ParticlesSchedulerScheduleEmitters",
            EShaderStage::Compute
        );

        m_SchedulerFinalizeShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSchedulerFinalize" },
            "ParticlesSchedulerFinalize",
            EShaderStage::Compute
        );

        SPipelineCreateInfo pipelineInfo{};
        pipelineInfo.Shader = m_SchedulerBeginShader;
        m_SchedulerBeginPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_SchedulerInitEmittersShader;
        m_SchedulerInitEmittersPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_SchedulerScheduleEmittersShader;
        m_SchedulerScheduleEmittersPipeline = ComputePipeline::Create(
            m_GraphicsContext,
            pipelineInfo
        );

        pipelineInfo.Shader = m_SchedulerFinalizeShader;
        m_SchedulerFinalizePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        CreateCoreV1ParticleStateLayoutRuntime(shaderLoader);
    }

    void Simulator::CreateCoreV1ParticleStateLayoutRuntime(const ShaderLoader* shaderLoader)
    {
        EE_CORE_ASSERT(
            m_ParticleStateLayouts.Find(EParticleStateLayout::CoreV1),
            "Aether requires a CoreV1 particle state layout descriptor."
        )

        EE_CORE_ASSERT(
            !FindParticleStateLayoutRuntime(EParticleStateLayout::CoreV1),
            "Aether cannot create the CoreV1 particle state runtime twice."
        )

        m_ParticleStateLayoutRuntimes.push_back({
            .Key = EParticleStateLayout::CoreV1,
        });

        auto& runtime = m_ParticleStateLayoutRuntimes.back();

        runtime.SpawnShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSpawn" },
            "ParticlesSpawn",
            EShaderStage::Compute
        );

        runtime.UpdateShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesUpdate" },
            "ParticlesUpdate",
            EShaderStage::Compute
        );

        SPipelineCreateInfo pipelineInfo{};
        pipelineInfo.Shader = runtime.SpawnShader;
        runtime.SpawnPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = runtime.UpdateShader;
        runtime.UpdatePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        runtime.StrandShader = shaderLoader->LoadShader(
            "./Shaders/Aether/", std::array<std::string_view, 1>{ "StrandsUpdate" },
            "StrandsUpdate", EShaderStage::Compute);
        if (!runtime.StrandShader) return;
        pipelineInfo.Shader = runtime.StrandShader;
        runtime.StrandPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);
    }

    void Simulator::CreateBuffers()
    {
        for (const auto& descriptor : m_ParticleStateLayouts.GetDescriptors())
        {
            auto* runtime = FindParticleStateLayoutRuntime(descriptor.Key);
            EE_CORE_ASSERT(runtime, "Particle state layout requires a simulation runtime.")
            if (!runtime) continue;

            runtime->ParticleStateBuffer = StorageBuffer::Create(
                m_GraphicsContext,
                descriptor.ParticleStateStride * descriptor.ParticleCapacity
            );
        }

        m_StrandPointBuffer = StorageBuffer::Create(m_GraphicsContext,
            3 * sizeof(glm::vec4) * std::max<size_t>(1, m_Limits.StrandPointCapacity));
        if (!m_StrandPointBuffer || !m_StrandPointBuffer->IsValid())
        {
            EE_CORE_ERROR("Could not allocate Aether strand point storage.");
            return;
        }
        m_StrandPointBuffer->Clear();

        m_EmitterStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUEmitterInstanceState) * m_Limits.EmitterCapacity
        );

        m_SpawnRequestBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUSpawnRequest) * m_Limits.EmitterCapacity
        );

        m_TriggerTargetBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUTriggerTarget) * m_Limits.TriggerTargetCapacity
        );

        for (auto& buffer : m_TriggerEventBuffers)
        {
            buffer = StorageBuffer::Create(
                m_GraphicsContext,
                sizeof(SGPUTriggerEvent) *
                    m_Limits.EmitterCapacity *
                    m_Limits.TriggerEventCapacityPerEmitter
            );
            buffer->Clear();
        }

        m_TriggerQueueStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUTriggerQueueState) * m_Limits.EmitterCapacity * 2
        );

        m_SystemInstanceBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUSystemInstance) * m_Limits.MaxSystemInstances
        );

        m_SystemSchedulerStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUSystemSchedulerState) * m_Limits.MaxSystemInstances
        );

        m_EmitterBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUEmitter) * m_Limits.EmitterCapacity
        );

        m_OperationBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUOperation) * m_Limits.OperationCapacity
        );

        m_OperandBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(uint32_t) * m_Limits.OperandCapacity
        );

        m_ConstantBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(uint32_t) * std::max<std::size_t>(1, m_Limits.ConstantCapacity)
        );

        m_ParameterBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(uint32_t) * std::max<std::size_t>(1, m_Limits.ParameterWordCapacity)
        );

        m_ParamsBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SParamsData)
        );

        m_EmitterStateBuffer->Clear();
        m_SpawnRequestBuffer->Clear();
        m_TriggerQueueStateBuffer->Clear();
        m_SystemSchedulerStateBuffer->Clear();
    }

    void Simulator::BindShaderParameters()
    {
        constexpr SSchedulePushConstants schedulePushConstants{};

        m_SchedulerBeginShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_SchedulerBeginShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_SchedulerBeginShader->BindStorageBuffer("schedulerStates", m_SystemSchedulerStateBuffer);

        m_SchedulerInitEmittersShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_SchedulerInitEmittersShader->BindStorageBuffer(
            "instances",
            m_SystemInstanceBuffer
        );
        m_SchedulerInitEmittersShader->BindStorageBuffer(
            "emitterStates",
            m_EmitterStateBuffer
        );
        m_SchedulerInitEmittersShader->BindStorageBuffer(
            "triggerQueueStates",
            m_TriggerQueueStateBuffer
        );
        m_SchedulerInitEmittersShader->BindStorageBuffer(
            "schedulerStates",
            m_SystemSchedulerStateBuffer
        );

        m_SchedulerScheduleEmittersShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "instances",
            m_SystemInstanceBuffer
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "emitters",
            m_EmitterBuffer
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "emitterStates",
            m_EmitterStateBuffer
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "spawnRequests",
            m_SpawnRequestBuffer
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "triggerTargets",
            m_TriggerTargetBuffer
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "triggerEventsA",
            m_TriggerEventBuffers[0]
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "triggerEventsB",
            m_TriggerEventBuffers[1]
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "triggerQueueStates",
            m_TriggerQueueStateBuffer
        );
        m_SchedulerScheduleEmittersShader->BindStorageBuffer(
            "schedulerStates",
            m_SystemSchedulerStateBuffer
        );
        m_SchedulerScheduleEmittersShader->BindConstantBuffer(
            "cbParams",
            m_ParamsBuffer
        );

        m_SchedulerFinalizeShader->SetPushConstant(
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );

        m_SchedulerFinalizeShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_SchedulerFinalizeShader->BindStorageBuffer("schedulerStates", m_SystemSchedulerStateBuffer);

        for (const auto& runtime : m_ParticleStateLayoutRuntimes)
            BindParticleStateLayoutShaderParameters(runtime);
    }

    void Simulator::BindParticleStateLayoutShaderParameters(
        const SParticleStateLayoutRuntime& runtime
    ) const
    {
        EE_CORE_ASSERT(
            runtime.ParticleStateBuffer,
            "Aether cannot bind and uninitialized particle state layout runtime."
        )
        if (!runtime.ParticleStateBuffer) return;

        if (runtime.StrandShader)
        {
            constexpr SStrandPushConstants strandPushConstants{};
            runtime.StrandShader->SetPushConstant("pc", (void*)&strandPushConstants, sizeof(strandPushConstants));
            runtime.StrandShader->BindStorageBuffer("particles", runtime.ParticleStateBuffer);
            runtime.StrandShader->BindStorageBuffer("strandPoints", m_StrandPointBuffer);
            runtime.StrandShader->BindStorageBuffer("emitters", m_EmitterBuffer);
            runtime.StrandShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
            runtime.StrandShader->BindStorageBuffer("operations", m_OperationBuffer);
            runtime.StrandShader->BindStorageBuffer("operands", m_OperandBuffer);
            runtime.StrandShader->BindStorageBuffer("constants", m_ConstantBuffer);
            runtime.StrandShader->BindStorageBuffer("parameters", m_ParameterBuffer);
            runtime.StrandShader->BindConstantBuffer("cbParams", m_ParamsBuffer);
        }

        constexpr SSpawnPushConstants spawnPushConstants{};

        runtime.SpawnShader->SetPushConstant(
            "pc",
            (void*)&spawnPushConstants,
            sizeof(spawnPushConstants)
        );

        runtime.SpawnShader->BindStorageBuffer("particles", runtime.ParticleStateBuffer);
        runtime.SpawnShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        runtime.SpawnShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        runtime.SpawnShader->BindStorageBuffer("spawnRequests", m_SpawnRequestBuffer);
        runtime.SpawnShader->BindStorageBuffer("operations", m_OperationBuffer);
        runtime.SpawnShader->BindStorageBuffer("operands", m_OperandBuffer);
        runtime.SpawnShader->BindStorageBuffer("constants", m_ConstantBuffer);
        runtime.SpawnShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        runtime.SpawnShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        constexpr SUpdatePushConstants updatePushConstants{};
        runtime.UpdateShader->SetPushConstant(
            "pc",
            (void*)&updatePushConstants,
            sizeof(updatePushConstants)
        );

        runtime.UpdateShader->BindStorageBuffer("particles", runtime.ParticleStateBuffer);
        runtime.UpdateShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        runtime.UpdateShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        runtime.UpdateShader->BindStorageBuffer("operations", m_OperationBuffer);
        runtime.UpdateShader->BindStorageBuffer("operands", m_OperandBuffer);
        runtime.UpdateShader->BindStorageBuffer("constants", m_ConstantBuffer);
        runtime.UpdateShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        runtime.UpdateShader->BindConstantBuffer("cbParams", m_ParamsBuffer);
    }

    Simulator::SInstanceRecord* Simulator::ResolveInstanceRecord(
        const SystemInstanceRenderProxy& proxy
    )
    {
        const auto found = m_InstanceRecords.find(proxy.GetKey());

        if (found != m_InstanceRecords.end() &&
            found->second.SystemInstanceRevision == proxy.GetRevision())
        {
            return &found->second;
        }

        const auto& system = proxy.GetCompiledSystem();

        const auto allocation = m_ResourcePool.Allocate(system);
        if (!allocation)
        {
            // A replacement can fail to fit beside its previous allocation even
            // when it fits by itself. Retire the stale allocation so a later
            // frame can retry after this frame slot's fence completes.
            if (found != m_InstanceRecords.end())
            {
                QueueRetirement(found->second.Allocation);
                m_InstanceRecords.erase(found);
            }

            if (m_AllocationFailures.insert(proxy.GetKey()).second)
            {
                EE_CORE_ERROR(
                    "Aether GPU resource pool exhausted for system '{}'.",
                    system.SourceId
                )
            }
            return nullptr;
        }

        ClearParticleAllocation(*allocation);
        UploadCompiledSystem(proxy, *allocation);

        const SInstanceRecord replacement{
            .SystemInstanceKey = proxy.GetKey(),
            .SystemInstanceRevision = proxy.GetRevision(),
            .CompiledSystemId = system.SourceId,
            .CompilationRevision = system.CompilationRevision,
            .ParameterRevision = proxy.GetParameterRevision(),
            .Allocation = *allocation,
        };

        if (found == m_InstanceRecords.end())
        {
            const auto [it, inserted] = m_InstanceRecords.emplace(proxy.GetKey(), replacement);

            EE_CORE_ASSERT(inserted, "Aether system instance registry insertion failed.")
            m_AllocationFailures.erase(proxy.GetKey());
            return &it->second;
        }

        // The replacement is fully allocated and uploaded before retiring the
        // previous record.
        QueueRetirement(found->second.Allocation);
        found->second = replacement;
        m_AllocationFailures.erase(proxy.GetKey());
        return &found->second;
    }

    void Simulator::UpdateBuffers(
        const SystemInstanceRenderProxy& proxy,
        SInstanceRecord& record
    )
    {
        if (record.ParameterRevision != proxy.GetParameterRevision())
        {
            UploadInstanceParameters(proxy, record.Allocation);
            record.ParameterRevision = proxy.GetParameterRevision();
        }

        const auto& system = proxy.GetCompiledSystem();
        const auto& allocation = record.Allocation;

        const SGPUSystemInstance data
        {
            .ParticleBaseOffset = allocation.Particles.Offset,
            .EmitterBaseOffset = allocation.Emitters.Offset,
            .OperationBaseOffset = allocation.Operations.Offset,
            .OperandBaseOffset = allocation.Operands.Offset,
            .ConstantBaseOffset = allocation.Constants.Offset,
            .ParameterBaseOffset = allocation.Parameters.Offset,
            .EmitterStateBaseOffset = allocation.EmitterStates.Offset,
            .SpawnRequestBaseOffset = allocation.SpawnRequests.Offset,
            .TriggerEventBaseOffset = allocation.TriggerEvents.Offset,
            .TriggerQueueStateBaseOffset = allocation.TriggerQueueStates.Offset,
            .ParticleCount = allocation.Particles.Count,
            .EmitterCount = allocation.Emitters.Count,
            .TriggerEventCapacityPerEmitter = m_Limits.TriggerEventCapacityPerEmitter,
            .Generation = allocation.Generation,
            .ParticleStateLayoutIndex = (uint32_t)system.ParticleStateLayout,
        };

        m_SystemInstanceBuffer->UpdateData(
            &data,
            sizeof(data),
            allocation.InstanceIndex * sizeof(data)
        );
    }

    void Simulator::UploadCompiledSystem(
        const SystemInstanceRenderProxy& proxy,
        const SSystemInstanceAllocation& allocation
    ) const
    {
        const auto& system = proxy.GetCompiledSystem();

        auto* emitters = (SGPUEmitter*)m_EmitterBuffer->Map();
        for (uint32_t i = 0; i < allocation.Emitters.Count; ++i)
        {
            emitters[allocation.Emitters.Offset + i] = ToEmitterData(
                system.Emitters[i],
                allocation.Operations.Offset,
                allocation.TriggerTargets.Offset
            );
        }

        // Shared references remain relative to the compiled program and the instance.
        if (allocation.UploadProgram)
        {
            if (allocation.Operations.Count)
                m_OperationBuffer->UpdateData(
                    system.ModuleData.Operations.data(),
                    allocation.Operations.Count * sizeof(SGPUOperation),
                    allocation.Operations.Offset * sizeof(SGPUOperation)
                );
            if (allocation.Operands.Count)
                m_OperandBuffer->UpdateData(
                    system.ModuleData.Operands.data(),
                    allocation.Operands.Count * sizeof(uint32_t),
                    allocation.Operands.Offset * sizeof(uint32_t)
                );
            if (allocation.Constants.Count)
                m_ConstantBuffer->UpdateData(
                    system.ModuleData.Constants.data(),
                    allocation.Constants.Count * sizeof(uint32_t),
                    allocation.Constants.Offset * sizeof(uint32_t)
                );
        }

        UploadInstanceParameters(proxy, allocation);

        auto* targets = (SGPUTriggerTarget*)m_TriggerTargetBuffer->Map();
        for (uint32_t i = 0; i < allocation.TriggerTargets.Count; ++i)
            targets[allocation.TriggerTargets.Offset + i] =
                ToTriggerTargetData(system.TriggerTargets[i]);
    }

    void Simulator::UploadInstanceParameters(
        const SystemInstanceRenderProxy& proxy,
        const SSystemInstanceAllocation& allocation
    ) const
    {
        const auto words = proxy.GetParameterWords();

        EE_CORE_ASSERT(
            words.size() == allocation.Parameters.Count,
            "Aether parameter upload does not match its allocation."
        )
        if (words.size() != allocation.Parameters.Count || words.empty())
            return;

        m_ParameterBuffer->UpdateData(
            words.data(),
            words.size_bytes(),
            std::size_t(allocation.Parameters.Offset) * sizeof(uint32_t)
        );
    }

    Simulator::SParticleStateLayoutRuntime* Simulator::FindParticleStateLayoutRuntime(
        const EParticleStateLayout layout
    )
    {
        for (auto& runtime : m_ParticleStateLayoutRuntimes)
            if (runtime.Key == layout) return &runtime;
        return nullptr;
    }

    const Simulator::SParticleStateLayoutRuntime* Simulator::FindParticleStateLayoutRuntime(
        const EParticleStateLayout layout
    ) const
    {
        for (const auto& runtime : m_ParticleStateLayoutRuntimes)
            if (runtime.Key == layout) return &runtime;
        return nullptr;
    }

    bool Simulator::IsParticleStateLayoutSupported(const EParticleStateLayout layout) const
    {
        const auto* runtime = FindParticleStateLayoutRuntime(layout);
        return runtime && runtime->IsReady();
    }

    std::vector<Simulator::SSubmittedSystemInstance> Simulator::ResolveSubmittedInstances(
        const FrameSubmission& submission
    )
    {
        std::vector<SSubmittedSystemInstance> instances;
        instances.reserve(submission.GetRenderProxies().size());

        for (const auto& proxy : submission.GetRenderProxies())
        {
            const auto& system = proxy->GetCompiledSystem();
            m_LastMetrics.RequestedEmitterCount += system.Emitters.size();
            m_LastMetrics.RequestedParticleCapacity += system.TotalMaxParticles;

            if (!IsParticleStateLayoutSupported(system.ParticleStateLayout))
            {
                if (m_UnsupportedParticleStateLayoutInstances.insert(proxy->GetKey()).second)
                {
                    EE_CORE_ERROR(
                        "Aether particle state layout '{}' is unsupported for system '{}'.",
                        (uint32_t)system.ParticleStateLayout,
                        system.SourceId
                    )
                }
                continue;
            }

            m_UnsupportedParticleStateLayoutInstances.erase(proxy->GetKey());

            auto* record = ResolveInstanceRecord(*proxy);
            if (!record) continue;

            UpdateBuffers(*proxy, *record);

            ++m_LastMetrics.SubmittedSystemInstanceCount;
            m_LastMetrics.SubmittedEmitterCount += record->Allocation.Emitters.Count;
            m_LastMetrics.SubmittedParticleCapacity += record->Allocation.Particles.Count;

            instances.push_back({
                .Proxy = proxy,
                .Allocation = record->Allocation,
                .ParticleStateLayout = system.ParticleStateLayout,
            });
        }

        return instances;
    }

    std::vector<Simulator::SSimulationBatch> Simulator::BuildSimulationBatches(
        const std::vector<SSubmittedSystemInstance>& instances
    )
    {
        std::vector<SSimulationBatch> batches;

        for (const auto& instance : instances)
        {
            auto found = std::ranges::find_if(batches, [&instance](const auto& batch)
            {
                return batch.ParticleStateLayout == instance.ParticleStateLayout;
            });

            if (found == batches.end())
            {
                batches.push_back({
                    .ParticleStateLayout = instance.ParticleStateLayout,
                });
                found = std::prev(batches.end());
            }

            found->Instances.push_back(&instance);
        }

        return batches;
    }

    std::vector<SParticleStateRenderResource> Simulator::BuildRenderResources() const
    {
        std::vector<SParticleStateRenderResource> resources;
        resources.reserve(m_ParticleStateLayoutRuntimes.size());

        for (const auto& runtime : m_ParticleStateLayoutRuntimes)
        {
            resources.push_back({
                .Layout = runtime.Key,
                .ParticleStateBuffer = runtime.ParticleStateBuffer,
                .StrandPointBuffer = m_StrandPointBuffer,
            });
        }

        return resources;
    }

    std::vector<SRenderItem> Simulator::BuildRenderItems(
        const std::vector<SSubmittedSystemInstance>& instances
    )
    {
        std::vector<SRenderItem> items;

        for (const auto& instance : instances)
        {
            const auto& emitters = instance.Proxy->GetCompiledSystem().Emitters;

            for (uint32_t emitterIndex = 0; emitterIndex < emitters.size(); ++emitterIndex)
            {
                const auto& emitter = emitters[emitterIndex];
                if (emitter.MaxParticles == 0) continue;

                items.push_back({
                    .Allocation = instance.Allocation,
                    .ParticleStateLayout = instance.ParticleStateLayout,
                    .RenderMode = emitter.RenderMode,
                    .Material = emitter.Material,
                    .WorldTransform = GetParticleRenderTransform(emitter, *instance.Proxy),
                    .EmitterIndex = emitterIndex,
                    .LocalParticleOffset = emitter.LocalParticleOffset,
                    .ParticleCount = emitter.MaxParticles,
                    .StrandPointOffset = instance.Allocation.StrandPoints.Offset + emitter.LocalStrandPointOffset,
                    .Strand = emitter.Strand,
                });
            }
        }

        return items;
    }

    void Simulator::SimulateBatch(
        const Ref<CommandBuffer>& cmd,
        const SSimulationBatch& batch
    )
    {
        const auto* runtime = FindParticleStateLayoutRuntime(batch.ParticleStateLayout);
        EE_CORE_ASSERT(runtime, "Aether particle state layout runtime is missing.")
        if (!runtime) return;

        const auto& particleBuffer = runtime->ParticleStateBuffer;

        // Scheduling: begin

        m_SchedulerBeginPipeline->Bind(cmd);

        for (const auto* instance : batch.Instances)
        {
            const SSchedulePushConstants pc{
                .InstanceIndex = instance->Allocation.InstanceIndex,
            };

            m_SchedulerBeginShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
            cmd->Dispatch(1);
        }

        BarrierSchedulingBuffers(cmd);

        // Scheduling: init emitters

        m_SchedulerInitEmittersPipeline->Bind(cmd);

        for (const auto* instance : batch.Instances)
        {
            const SSchedulePushConstants pc{
                .InstanceIndex = instance->Allocation.InstanceIndex,
            };

            m_SchedulerInitEmittersShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
            cmd->Dispatch((instance->Allocation.Emitters.Count + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }

        BarrierSchedulingBuffers(cmd);

        // Scheduling: generate spawn requests

        m_SchedulerScheduleEmittersPipeline->Bind(cmd);

        for (const auto* instance : batch.Instances)
        {
            const SSchedulePushConstants pc{
                .InstanceIndex = instance->Allocation.InstanceIndex,
            };

            m_SchedulerScheduleEmittersShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
            cmd->Dispatch((instance->Allocation.Emitters.Count + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }

        BarrierSchedulingBuffers(cmd);

        // Scheduling: release trigger events

        m_SchedulerFinalizePipeline->Bind(cmd);

        for (const auto* instance : batch.Instances)
        {
            const SSchedulePushConstants pc{
                .InstanceIndex = instance->Allocation.InstanceIndex,
            };

            m_SchedulerFinalizeShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
            cmd->Dispatch(1);
        }

        BarrierSchedulingBuffers(cmd);

        // Spawning

        particleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        runtime->SpawnPipeline->Bind(cmd);

        for (const auto* instance : batch.Instances)
        {
            const auto& system = instance->Proxy->GetCompiledSystem();
            const auto emitterCount = instance->Allocation.Emitters.Count;

            m_LastMetrics.ScheduledEmitterCount += emitterCount;

            for (uint32_t i = 0; i < emitterCount; ++i)
            {
                const auto maxParticles = system.Emitters[i].MaxParticles;
                if (maxParticles == 0) continue;

                ++m_LastMetrics.SpawnDispatchCount;

                const SSpawnPushConstants pc
                {
                    .InstanceIndex = instance->Allocation.InstanceIndex,
                    .EmitterIndex = i,
                };

                runtime->SpawnShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
                cmd->Dispatch((maxParticles + SPAWN_GROUP_SIZE - 1) / SPAWN_GROUP_SIZE);
            }
        }

        // Updating

        particleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        runtime->UpdatePipeline->Bind(cmd);

        for (const auto* instance : batch.Instances)
        {
            const SUpdatePushConstants pc
            {
                .InstanceIndex = instance->Allocation.InstanceIndex,
            };

            runtime->UpdateShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
            cmd->Dispatch((instance->Allocation.Particles.Count + UPDATE_GROUP_SIZE - 1) / UPDATE_GROUP_SIZE);
        }

        // Strand roots consume the updated particle positions; points persist across steps.
        particleBuffer->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderRead);
        m_StrandPointBuffer->Barrier(cmd, EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite);
        runtime->StrandPipeline->Bind(cmd);
        for (const auto* instance : batch.Instances)
        {
            for (const auto& emitter : instance->Proxy->GetCompiledSystem().Emitters)
            {
                if (emitter.RenderMode != EParticleRenderMode::Strand || emitter.MaxParticles == 0) continue;
                const auto& settings = emitter.Strand;
                const SStrandPushConstants pc{
                    .RootTransform = GetParticleRenderTransform(emitter, *instance->Proxy),
                    .ParticleOffset = instance->Allocation.Particles.Offset + emitter.LocalParticleOffset,
                    .PointOffset = instance->Allocation.StrandPoints.Offset + emitter.LocalStrandPointOffset,
                    .PointCount = settings.PointCount,
                    .StrandCount = emitter.MaxParticles,
                    .SegmentLength = settings.SegmentLength,
                    .Stiffness = settings.Stiffness,
                    .Damping = settings.Damping,
                    .Generation = instance->Allocation.Generation,
                    .SolverIterations = settings.SolverIterations,
                    .InstanceIndex = instance->Allocation.InstanceIndex,
                    .EmitterIndex = instance->Allocation.Emitters.Offset + uint32_t(&emitter - instance->Proxy->GetCompiledSystem().Emitters.data()),
                };
                runtime->StrandShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
                cmd->Dispatch((emitter.MaxParticles + UPDATE_GROUP_SIZE - 1) / UPDATE_GROUP_SIZE);
            }
        }
    }

    void Simulator::PublishRenderBarrier(
        const Ref<CommandBuffer>& cmd,
        const EParticleStateLayout layout
    ) const
    {
        const auto* runtime = FindParticleStateLayoutRuntime(layout);
        EE_CORE_ASSERT(runtime, "Aether particle state layout runtime is missing.")
        if (!runtime) return;

        m_StrandPointBuffer->Barrier(cmd, EPipelineStage::VertexShader, EPipelineAccess::ShaderRead);
        runtime->ParticleStateBuffer->Barrier(
            cmd,
            EPipelineStage::VertexShader | EPipelineStage::VertexInput,
            EPipelineAccess::ShaderRead | EPipelineAccess::VertexAttributeRead
        );
    }

    void Simulator::QueueRetirement(SSystemInstanceAllocation allocation)
    {
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        m_DeferredRetirements[frameIndex].push_back(std::move(allocation));
    }

    void Simulator::ProcessCompletedRetirements()
    {
        // Update() runs after GraphicsContext::Prepare() waited for this frame slot's fence.
        // All GPU work that used these allocations has therefore completed.
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        auto& retirements = m_DeferredRetirements[frameIndex];

        for (const auto& allocation : retirements)
            m_ResourcePool.Release(allocation);

        retirements.clear();
    }

    void Simulator::BarrierSchedulingBuffers(const Ref<CommandBuffer>& cmd) const
    {
        constexpr auto stage = EPipelineStage::ComputeShader;
        constexpr auto access = EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite;

        m_EmitterStateBuffer->Barrier(cmd, stage, access);
        m_SpawnRequestBuffer->Barrier(cmd, stage, access);
        m_TriggerEventBuffers[0]->Barrier(cmd, stage, access);
        m_TriggerEventBuffers[1]->Barrier(cmd, stage, access);
        m_TriggerQueueStateBuffer->Barrier(cmd, stage, access);
        m_SystemSchedulerStateBuffer->Barrier(cmd, stage, access);
    }

    void Simulator::ClearParticleAllocation(const SSystemInstanceAllocation& allocation)
    {
        const auto* layout = m_ParticleStateLayouts.Find(allocation.ParticleStateLayout);
        const auto* runtime = FindParticleStateLayoutRuntime(allocation.ParticleStateLayout);
        if (!layout || !runtime) return;

        runtime->ParticleStateBuffer->Fill(
            0,
            int32_t(allocation.Particles.Offset * layout->ParticleStateStride),
            allocation.Particles.Count * layout->ParticleStateStride
        );
        if (allocation.StrandPoints.Count != 0)
            m_StrandPointBuffer->Fill(0,
                int32_t(allocation.StrandPoints.Offset * 3 * sizeof(glm::vec4)),
                allocation.StrandPoints.Count * 3 * sizeof(glm::vec4));
    }
}
