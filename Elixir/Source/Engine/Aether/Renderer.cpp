#include "epch.h"
#include "Renderer.h"

#include "Engine/Graphics/CommandBuffer.h"

namespace Elixir::Aether
{
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    struct SSpritePushConstants
    {
        glm::mat4 WorldTransform{ 1.0f };
        uint32_t MaterialIndex = UINT32_MAX;
        uint32_t SpriteIndex = 0;
    };

    struct SMeshPushConstants
    {
        glm::mat4 WorldTransform{ 1.0f };
        uint32_t MaterialIndex = UINT32_MAX;
    };

    struct SRibbonPushConstants
    {
        glm::mat4 WorldTransform{ 1.0f };
        uint32_t EmitterIndex = 0;
        uint32_t ParticleBaseOffset = 0;
        uint32_t MaterialIndex = UINT32_MAX;
    };

    SEmitterData ToEmitterDescription(
        const SCompiledEmitter& emitter,
        uint32_t opBaseOffset,
        uint32_t triggerTargetBaseOffset
    )
    {
        SEmitterData desc{};
        desc.MetaA = {
            emitter.LocalParticleOffset,
            emitter.MaxParticles,
            opBaseOffset + emitter.SpawnOpOffset,
            emitter.SpawnOpCount
        };

        desc.MetaB = {
            opBaseOffset + emitter.UpdateOpOffset,
            emitter.UpdateOpCount,
            triggerTargetBaseOffset + emitter.TriggerTargetOffset,
            emitter.TriggerTargetCount
        };

        desc.MetaC = {
            (float)emitter.RenderMode,
            emitter.SpawnRatePerSecond,
            emitter.GravityScale,
            emitter.BurstIntervalSeconds
        };

        desc.MetaD = {
            (float)emitter.BurstCount,
            emitter.IsTriggerDriven ? 1.0f : 0.0f,
            0.0f,
            0.0f
        };

        return desc;
    }

    STriggerTargetData ToTriggerTargetDescription(const SCompiledTriggerTarget& target)
    {
        return {
            .TargetEmitterIndex = target.TargetEmitterIndex,
            .BurstCount = target.BurstCount,
            .DelaySeconds = target.DelaySeconds,
        };
    }

    SParticleOpData ToOpDescription(const SGPUParticleOp& op, uint32_t parameterBaseOffset)
    {
        const auto ResolveParameterIndex = [parameterBaseOffset](const uint32_t parameterIndex)
        {
            return parameterIndex == UINT32_MAX
                ? -1.0f
                : (float)(parameterBaseOffset + parameterIndex);
        };

        SParticleOpData desc{};

        desc.Header = {
            (float)(uint32_t)op.Type,
            (float)op.Target,
            ResolveParameterIndex(op.Parameter0Index),
            ResolveParameterIndex(op.Parameter1Index)
        };

        desc.Data0 = op.Data0;
        desc.Data1 = op.Data1;
        desc.Data2 = op.Data2;

        return desc;
    }

    SParameterData ToParameterDescription(const SGPUParameter& parameter)
    {
        SParameterData desc{};
        desc.Value = parameter.Value;

        return desc;
    }

    uint32_t GetRenderModeOrder(const EParticleRenderMode mode)
    {
        switch (mode)
        {
            case EParticleRenderMode::Mesh:     return 0;
            case EParticleRenderMode::Ribbon:   return 1;
            case EParticleRenderMode::Sprite:   return 2;
        }

        return UINT32_MAX;
    }

    glm::mat4 GetParticleRenderTransform(
        const SCompiledEmitter& emitter,
        const SystemInstance& instance
    )
    {
        return emitter.SimulationSpace == EParticleSimulationSpace::Local
            ? instance.GetWorldTransform()
            : glm::mat4{ 1.0f };
    }

    bool TryToGetParticleMaterialPass(
        const EParticleRenderMode renderMode,
        EMaterialPass& pass
    )
    {
        switch (renderMode)
        {
            case EParticleRenderMode::Sprite:
                pass = EMaterialPass::ParticleSprite;
                return true;
            case EParticleRenderMode::Ribbon:
                pass = EMaterialPass::ParticleRibbon;
                return true;
            case EParticleRenderMode::Mesh:
                pass = EMaterialPass::ParticleMesh;
                return true;
        }

        return false;
    }

    Renderer::Renderer(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const SParticlePoolLimits& limits
    ) : m_ParticlePoolLimits(limits),
        m_ParticleStateLayouts(m_ParticlePoolLimits.ParticleCapacity),
        m_ParticleResourcePool(m_ParticlePoolLimits, m_ParticleStateLayouts),
        m_MaterialSystem(CreateRef<MaterialSystem>(
            context,
            shaderLoader,
            limits.MaterialCapacity
        )),
        m_GraphicsContext(context)
    {
        static_assert(sizeof(SGPUParticleState) == PARTICLE_STATE_CORE_V1_STRIDE);
        EE_CORE_INFO("Initializing Aether Renderer.")

        Init(shaderLoader);
        CreateBuffers();
        InitPerFrameData();
        BindShaderParameters();
    }

    void Renderer::Update(const Timestep& timestep)
    {
        ProcessCompletedRetirements();

        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    void Renderer::Render(const FrameSubmission& submission, const Camera& camera)
    {
        const auto& instances = submission.GetInstances();

        m_LastSubmissionMetrics = {
            .SubmissionSerial = ++m_SubmissionSerial,
            .DeltaTimeSeconds = m_LastDeltaTimeSeconds,
            .ElapsedTimeSeconds = m_ElapsedTimeSeconds,
            .RequestedSystemInstanceCount = instances.size(),
            .TriggerEventCapacityPerEmitter =
                m_ParticlePoolLimits.TriggerEventCapacityPerEmitter,
        };

        m_RenderExtent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        m_FrameData.View = camera.GetViewMatrix();
        m_FrameData.Proj = camera.GetProjectionMatrix();
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameData.CameraPos = camera.GetPosition();
        m_FrameData.Time = m_ElapsedTimeSeconds;
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        std::vector<SSubmittedSystemInstance> submittedInstances;
        submittedInstances.reserve(instances.size());

        for (const auto* instance : instances)
        {
            const auto& system = instance->GetCompiledSystem();

            m_LastSubmissionMetrics.RequestedEmitterCount += system.Emitters.size();
            m_LastSubmissionMetrics.RequestedParticleCapacity += system.TotalMaxParticles;

            if (!IsParticleStateLayoutSupported(system.ParticleStateLayout))
            {
                if (m_UnsupportedParticleStateLayoutInstances.insert(instance->GetId()).second)
                {
                    EE_CORE_ERROR(
                        "Aether does not support particle state layout '{}' for system instance '{}'.",
                        (uint32_t)system.ParticleStateLayout,
                        system.Name
                    )
                }

                continue;
            }

            m_UnsupportedParticleStateLayoutInstances.erase(instance->GetId());

            auto* record = ResolveInstanceRecord(*instance);
            if (!record) continue;

            UpdateBuffers(*instance, *record);

            const auto emitterCount = record->Allocation.Emitters.Count;
            const auto particleCount = record->Allocation.Particles.Count;

            ++m_LastSubmissionMetrics.SubmittedSystemInstanceCount;
            m_LastSubmissionMetrics.SubmittedEmitterCount += emitterCount;
            m_LastSubmissionMetrics.SubmittedParticleCapacity += particleCount;

            submittedInstances.push_back({
                .Instance = instance,
                .Allocation = record->Allocation,
                .ParticleStateLayout = system.ParticleStateLayout,
            });
        }

        if (submittedInstances.empty())
            return;

        const auto materialInputs = CollectMaterialFrameInputs(submittedInstances);
        const auto materialSnapshot = m_MaterialSystem->BuildFrameSnapshot(
            materialInputs.Materials,
            materialInputs.Textures,
            m_SubmissionSerial
        );

        const auto simulationBatches = BuildSimulationBatches(submittedInstances);

        auto renderBatches = BuildRenderBatches(submittedInstances, materialSnapshot);
        PrepareMaterialBatches(renderBatches);

        m_LastSubmissionMetrics.SimulationBatchCount = simulationBatches.size();
        m_LastSubmissionMetrics.RenderBatchCount = renderBatches.size();
        m_LastSubmissionMetrics.SubmittedMaterialCount = materialSnapshot.MaterialCount;

        for (const auto& batch : renderBatches)
            m_LastSubmissionMetrics.SubmittedRenderItemCount += batch.Items.size();

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        for (const auto& batch : simulationBatches)
            SimulateBatch(cmd, batch);

        for (const auto& batch : simulationBatches)
        {
            const auto* runtime = FindParticleStateLayoutRuntime(batch.ParticleStateLayout);
            EE_CORE_ASSERT(runtime, "Aether particle state layout runtime is missing.")
            if (!runtime) continue;

            runtime->ParticleStateBuffer->Barrier(
                cmd,
                EPipelineStage::VertexShader | EPipelineStage::VertexInput,
                EPipelineAccess::ShaderRead | EPipelineAccess::VertexAttributeRead
            );
        }

        BeginRendering(cmd);

        for (const auto& batch : renderBatches)
            RenderBatch(cmd, batch);

        EndRendering(cmd);
    }

    void Renderer::Retire(const SystemInstance& instance)
    {
        const auto found = m_InstanceRecords.find(instance.GetId());
        if (found == m_InstanceRecords.end())
            return;

        QueueRetirement(found->second.Allocation);
        m_InstanceRecords.erase(found);
        m_AllocationFailures.erase(instance.GetId());
        m_UnsupportedParticleStateLayoutInstances.erase(instance.GetId());
    }

    const SParticleSubmissionMetrics& Renderer::GetLastSubmissionMetrics() const
    {
        return m_LastSubmissionMetrics;
    }

    void Renderer::Init(const ShaderLoader* shaderLoader)
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

    void Renderer::CreateCoreV1ParticleStateLayoutRuntime(const ShaderLoader* shaderLoader)
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

        runtime.SpriteVertexLayout = {{
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        }};

        runtime.MeshVertexLayout = {{
            {
                {
                    { EDataType::Vec3,  "Position" },
                    { EDataType::Vec3,  "Normal"   },
                },
                EInputRate::Vertex
            },
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        }};
    }

    void Renderer::CreateBuffers()
    {
        for (const auto& descriptor : m_ParticleStateLayouts.GetDescriptors())
        {
            auto* runtime = FindParticleStateLayoutRuntime(descriptor.Key);
            EE_CORE_ASSERT(
                runtime,
                "Every particle state layout descriptor requires a renderer runtime."
            )
            if (!runtime) continue;

            runtime->ParticleStateBuffer = StorageBuffer::Create(
                m_GraphicsContext,
                descriptor.ParticleStateStride * descriptor.ParticleCapacity
            );
        }

        EE_CORE_ASSERT(
            m_ParticleStateLayoutRuntimes.size() ==
                m_ParticleStateLayouts.GetDescriptors().size(),
            "Every particle state layout runtime requires a registered descriptor."
        )

        EE_CORE_ASSERT(
            FindParticleStateLayoutRuntime(EParticleStateLayout::CoreV1),
            "Aether requires a CoreV1 particle state layout runtime."
        )

        m_EmitterStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SEmitterInstanceStateData) * m_ParticlePoolLimits.EmitterCapacity
        );

        m_SpawnRequestBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SSpawnRequestData) * m_ParticlePoolLimits.EmitterCapacity
        );

        m_TriggerTargetBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(STriggerTargetData) * m_ParticlePoolLimits.TriggerTargetCapacity
        );

        for (auto& buffer : m_TriggerEventBuffers)
        {
            buffer = StorageBuffer::Create(
                m_GraphicsContext,
                sizeof(STriggerEventData) *
                    m_ParticlePoolLimits.EmitterCapacity *
                    m_ParticlePoolLimits.TriggerEventCapacityPerEmitter
            );
            buffer->Clear();
        }

        m_TriggerQueueStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(STriggerQueueStateData) * m_ParticlePoolLimits.EmitterCapacity * 2
        );

        m_SystemInstanceBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SSystemInstanceData) * m_ParticlePoolLimits.MaxSystemInstances
        );

        m_SystemSchedulerStateBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SSystemSchedulerStateData) * m_ParticlePoolLimits.MaxSystemInstances
        );

        m_EmitterStateBuffer->Clear();
        m_SpawnRequestBuffer->Clear();
        m_TriggerQueueStateBuffer->Clear();
        m_SystemSchedulerStateBuffer->Clear();

        m_EmitterBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SEmitterData) * m_ParticlePoolLimits.EmitterCapacity
        );

        m_OpBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SParticleOpData) * m_ParticlePoolLimits.OpCapacity
        );

        m_ParameterBuffer = DynamicStorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SParameterData) * m_ParticlePoolLimits.ParameterCapacity
        );

        m_ParamsBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SParamsData)
        );

        CreateMeshVertexBuffer();
    }

    void Renderer::CreateMeshVertexBuffer()
    {
        static constexpr std::array<MeshVertex, 36> vertices = {{
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},

            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},

            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
        }};

        m_MeshVertexCount = (uint32_t)vertices.size();
        m_MeshVertexBuffer = VertexBuffer::Create(
            m_GraphicsContext,
            sizeof(MeshVertex) * vertices.size(),
            vertices.data()
        );

        const auto* coreV1Runtime = FindParticleStateLayoutRuntime(EParticleStateLayout::CoreV1);
        EE_CORE_ASSERT(
            coreV1Runtime,
            "Aether CoreV1 particle state runtime is missing."
        )
        if (!coreV1Runtime) return;

        m_MeshVertexBuffer->SetLayout(coreV1Runtime->MeshVertexLayout);
    }

    void Renderer::InitPerFrameData()
    {
        m_FrameConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(SFrameData),
            &m_FrameData
        );
    }

    void Renderer::BindShaderParameters()
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

        for (auto& runtime : m_ParticleStateLayoutRuntimes)
            BindParticleStateLayoutShaderParameters(runtime);
    }

    void Renderer::BindParticleStateLayoutShaderParameters(
        const SParticleStateLayoutRuntime& runtime
    ) const
    {
        EE_CORE_ASSERT(
            runtime.ParticleStateBuffer,
            "Aether cannot bind and uninitialized particle state layout runtime."
        )
        if (!runtime.ParticleStateBuffer) return;

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
        runtime.SpawnShader->BindStorageBuffer("ops", m_OpBuffer);
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
        runtime.UpdateShader->BindStorageBuffer("ops", m_OpBuffer);
        runtime.UpdateShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        runtime.UpdateShader->BindConstantBuffer("cbParams", m_ParamsBuffer);
    }

    void Renderer::BeginRendering(const Ref<CommandBuffer>& cmd) const
    {
        const auto renderingInfo = SRenderingInfo
        {
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        };

        Viewport viewport = {};
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = (float)m_RenderExtent.Width;
        viewport.Height = (float)m_RenderExtent.Height;
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

    Renderer::SInstanceRecord* Renderer::ResolveInstanceRecord(const SystemInstance& instance)
    {
        const auto instanceRevision = instance.GetRevision();
        const auto found = m_InstanceRecords.find(instance.GetId());

        if (found != m_InstanceRecords.end() &&
            found->second.SystemInstanceRevision == instanceRevision)
        {
            return &found->second;
        }

        const auto& system = instance.GetCompiledSystem();

        const auto replacementAllocation = m_ParticleResourcePool.Allocate(system);
        if (!replacementAllocation)
        {
            if (m_AllocationFailures.insert(instance.GetId()).second)
            {
                EE_CORE_ERROR(
                    "Aether GPU resource pool exhausted while creating system instance '{}'.",
                    system.Name
                )
            }

            return nullptr;
        }

        ClearParticleAllocation(*replacementAllocation);
        UploadCompiledSystem(instance, *replacementAllocation);

        const SInstanceRecord replacement{
            .SystemInstanceId = instance.GetId(),
            .SystemInstanceRevision = instanceRevision,
            .CompiledSystemId = system.SourceId,
            .CompilationRevision = system.CompilationRevision,
            .ParameterRevision = instance.GetParameterRevision(),
            .Allocation = *replacementAllocation,
        };

        if (found == m_InstanceRecords.end())
        {
            const auto [it, inserted] = m_InstanceRecords.emplace(instance.GetId(), replacement);

            EE_CORE_ASSERT(inserted, "Aether system instance registry insertion failed.")
            m_AllocationFailures.erase(instance.GetId());
            return &it->second;
        }

        // The replacement is fully allocated and uploaded before retiring the
        // previous record. If allocation fails, the old record remains intact.
        QueueRetirement(found->second.Allocation);
        found->second = replacement;
        m_AllocationFailures.erase(instance.GetId());
        return &found->second;
    }

    void Renderer::UploadCompiledSystem(
        const SystemInstance& instance,
        const SSystemInstanceAllocation& allocation
    ) const
    {
        const auto& system = instance.GetCompiledSystem();

        auto* emitters = (SEmitterData*)m_EmitterBuffer->Map();
        for (uint32_t i = 0; i < allocation.Emitters.Count; ++i)
        {
            emitters[allocation.Emitters.Offset + i] = ToEmitterDescription(
                system.Emitters[i],
                allocation.Ops.Offset,
                allocation.TriggerTargets.Offset
            );
        }

        auto* ops = (SParticleOpData*)m_OpBuffer->Map();
        for (uint32_t i = 0; i < allocation.Ops.Count; ++i)
        {
            ops[allocation.Ops.Offset + i] = ToOpDescription(
                system.Ops[i],
                allocation.Parameters.Offset
            );
        }

        UploadInstanceParameters(instance, allocation);

        auto* targets = (STriggerTargetData*)m_TriggerTargetBuffer->Map();
        for (uint32_t i = 0; i < allocation.TriggerTargets.Count; ++i)
            targets[allocation.TriggerTargets.Offset + i] =
                ToTriggerTargetDescription(system.TriggerTargets[i]);
    }

    void Renderer::UploadInstanceParameters(
        const SystemInstance& instance,
        const SSystemInstanceAllocation& allocation
    ) const
    {
        auto* parameters = (SParameterData*)m_ParameterBuffer->Map();
        for (uint32_t i = 0; i < allocation.Parameters.Count; ++i)
            parameters[allocation.Parameters.Offset + i].Value =
                instance.ResolveParameterValue(i);
    }

    void Renderer::QueueRetirement(SSystemInstanceAllocation allocation)
    {
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        m_DeferredRetirements[frameIndex].push_back(std::move(allocation));
    }

    void Renderer::ProcessCompletedRetirements()
    {
        // Update() runs after GraphicsContext::Prepare() waited for this frame slot's fence.
        // All GPU work that used these allocations has therefore completed.
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        auto& retirements = m_DeferredRetirements[frameIndex];

        for (const auto& allocation : retirements)
            m_ParticleResourcePool.Release(allocation);

        retirements.clear();
    }

    void Renderer::UpdateBuffers(SystemInstance const& instance, SInstanceRecord& record)
    {
        if (record.ParameterRevision != instance.GetParameterRevision())
        {
            UploadInstanceParameters(instance, record.Allocation);
            record.ParameterRevision = instance.GetParameterRevision();
        }

        const SParamsData params{
            .Time = { m_LastDeltaTimeSeconds, m_ElapsedTimeSeconds, 0.0f, 0.0f },
            .Viewport = {
                (float)m_RenderExtent.Width,
                (float)m_RenderExtent.Height,
                0.0f,
                0.0f
            }
        };
        m_ParamsBuffer->UpdateData(&params, sizeof(SParamsData));

        const auto& system = instance.GetCompiledSystem();

        const SSystemInstanceData instanceData
        {
            .ParticleBaseOffset = record.Allocation.Particles.Offset,
            .EmitterBaseOffset = record.Allocation.Emitters.Offset,
            .OpBaseOffset = record.Allocation.Ops.Offset,
            .ParameterBaseOffset = record.Allocation.Parameters.Offset,
            .EmitterStateBaseOffset = record.Allocation.EmitterStates.Offset,
            .SpawnRequestBaseOffset = record.Allocation.SpawnRequests.Offset,
            .TriggerEventBaseOffset = record.Allocation.TriggerEvents.Offset,
            .TriggerQueueStateBaseOffset = record.Allocation.TriggerQueueStates.Offset,
            .ParticleCount = record.Allocation.Particles.Count,
            .EmitterCount = record.Allocation.Emitters.Count,
            .TriggerEventCapacityPerEmitter = m_ParticlePoolLimits.TriggerEventCapacityPerEmitter,
            .Generation = record.Allocation.Generation,
            .ParticleStateLayoutIndex = (uint32_t)system.ParticleStateLayout,
        };

        m_SystemInstanceBuffer->UpdateData(
            &instanceData,
            sizeof(SSystemInstanceData),
            record.Allocation.InstanceIndex * sizeof(SSystemInstanceData)
        );
    }

    Renderer::SParticleStateLayoutRuntime* Renderer::FindParticleStateLayoutRuntime(
        const EParticleStateLayout layout
    )
    {
        for (auto& runtime : m_ParticleStateLayoutRuntimes)
        {
            if (runtime.Key == layout)
                return &runtime;
        }

        return nullptr;
    }

    const Renderer::SParticleStateLayoutRuntime* Renderer::FindParticleStateLayoutRuntime(
        const EParticleStateLayout layout
    ) const
    {
        for (const auto& runtime : m_ParticleStateLayoutRuntimes)
        {
            if (runtime.Key == layout)
                return &runtime;
        }

        return nullptr;
    }

    bool Renderer::IsParticleStateLayoutSupported(const EParticleStateLayout layout) const
    {
        const auto* runtime = FindParticleStateLayoutRuntime(layout);
        return runtime && runtime->IsReady();
    }

    std::vector<Renderer::SSimulationBatch> Renderer::BuildSimulationBatches(
        const std::vector<SSubmittedSystemInstance>& instances
    ) const
    {
        std::vector<SSimulationBatch> batches;

        for (const auto& instance : instances)
        {
            SSimulationBatch* batch = nullptr;

            for (auto& candidate : batches)
            {
                if (candidate.ParticleStateLayout != instance.ParticleStateLayout)
                    continue;

                batch = &candidate;
                break;
            }

            if (!batch)
            {
                batches.push_back({
                    .ParticleStateLayout = instance.ParticleStateLayout,
                });
                batch = &batches.back();
            }

            batch->Instances.push_back(&instance);
        }

        return batches;
    }

    std::vector<Renderer::SRenderBatch> Renderer::BuildRenderBatches(
        const std::vector<SSubmittedSystemInstance>& instances,
        const SMaterialFrameSnapshot& materials
    )
    {
        std::vector<SRenderBatch> batches;

        for (const auto& instance : instances)
        {
            const auto& system = instance.Instance->GetCompiledSystem();

            for (uint32_t emitterIndex = 0; emitterIndex < system.Emitters.size(); ++emitterIndex)
            {
                const auto& emitter = system.Emitters[emitterIndex];

                if (emitter.MaxParticles == 0)
                    continue;

                EMaterialPass materialPass;
                if (!TryToGetParticleMaterialPass(emitter.RenderMode, materialPass))
                    continue;

                const auto& materialRef = m_MaterialSystem->ResolveParticleMaterial(
                    materialPass,
                    emitter.Material
                );

                const auto* material = materialRef.get();
                uint32_t spriteIndex = m_MaterialSystem->GetFallbackTextureIndex();

                if (emitter.RenderMode == EParticleRenderMode::Sprite)
                    spriteIndex = m_MaterialSystem->FindTextureIndex(emitter.SpriteTexture);

                const auto program = m_MaterialSystem->GetProgramKey(
                    materialPass,
                    *material
                );

                EE_CORE_ASSERT(
                    program,
                    "Resolved particle material must provide its shader permutation."
                )
                if (!program) continue;

                const auto index = materials.Table->Find(*material);
                EE_CORE_ASSERT(index, "Material frame snapshot is missing an emitter material.")
                if (!index) continue;

                const SRenderBatchKey key{
                    .ParticleStateLayout = instance.ParticleStateLayout,
                    .RenderMode = emitter.RenderMode,
                    .MaterialProgram = *program,
                };

                SRenderBatch* batch = nullptr;

                for (auto& candidate : batches)
                {
                    if (candidate.Key != key)
                        continue;

                    batch = &candidate;
                    break;
                }

                if (!batch)
                {
                    batches.push_back({
                        .Key = key,
                    });
                    batch = &batches.back();
                }

                batch->Items.push_back({
                    .Instance = &instance,
                    .Emitter = &emitter,
                    .Material = material,
                    .MaterialIndex = *index,
                    .SpriteIndex = spriteIndex,
                    .LocalEmitterIndex = emitterIndex,
                });
            }
        }

        std::ranges::stable_sort(batches, [](const SRenderBatch& left, const SRenderBatch& right)
        {
            if (left.Key.ParticleStateLayout != right.Key.ParticleStateLayout)
            {
                return uint32_t(left.Key.ParticleStateLayout) <
                    uint32_t(right.Key.ParticleStateLayout);
            }

            if (left.Key.RenderMode != right.Key.RenderMode)
            {
                return GetRenderModeOrder(left.Key.RenderMode) <
                    GetRenderModeOrder(right.Key.RenderMode);
            }

            return std::less<const void*>{}(
                left.Key.MaterialProgram.Identity,
                right.Key.MaterialProgram.Identity
            );
        });

        return batches;
    }

    void Renderer::PrepareMaterialBatches(std::vector<SRenderBatch>& batches)
    {
        static const BufferLayout ribbonVertexLayout;

        const std::array constantBuffers{
            SMaterialConstantBufferBinding{
                .Name = "cbFrame",
                .Buffer = m_FrameConstantBuffer,
            },
        };

        for (auto& batch : batches)
        {
            if (!batch.Key.MaterialProgram || batch.Items.empty())
                continue;

            const auto* runtime = FindParticleStateLayoutRuntime(batch.Key.ParticleStateLayout);
            EE_CORE_ASSERT(runtime, "Aether particle state layout runtime is missing.")
            if (!runtime) continue;

            const auto* material = batch.Items.front().Material;

            switch (batch.Key.RenderMode)
            {
                case EParticleRenderMode::Sprite:
                {
                    const SSpritePushConstants initial{
                        .SpriteIndex = m_MaterialSystem->GetFallbackTextureIndex(),
                    };

                    batch.PreparedMaterial = m_MaterialSystem->PrepareMaterialPass({
                        .Pass = EMaterialPass::ParticleSprite,
                        .Material = material,
                        .Pipeline = {
                            .VertexLayoutKey = uint64_t(runtime->Key),
                            .VertexLayout = &runtime->SpriteVertexLayout,
                        },
                        .ExternalResources = {
                            .ConstantBuffers = constantBuffers,
                        },
                        .InitialPushConstants = std::as_bytes(std::span{ &initial, size_t{ 1 }}),
                    });
                    break;
                }

                case EParticleRenderMode::Ribbon:
                {
                    const SRibbonPushConstants initial{};

                    const std::array storageBuffers{
                        SMaterialStorageBufferBinding{
                            .Name = "particles",
                            .Buffer = MaterialStorageBuffer{ runtime->ParticleStateBuffer },
                        },
                        SMaterialStorageBufferBinding{
                            .Name = "emitters",
                            .Buffer = MaterialStorageBuffer{ m_EmitterBuffer },
                        },
                    };

                    batch.PreparedMaterial = m_MaterialSystem->PrepareMaterialPass({
                        .Pass = EMaterialPass::ParticleRibbon,
                        .Material = material,
                        .Pipeline = {
                            .VertexLayoutKey = uint64_t(runtime->Key),
                            .VertexLayout = &ribbonVertexLayout,
                        },
                        .ExternalResources = {
                            .ConstantBuffers = constantBuffers,
                            .StorageBuffers = storageBuffers,
                        },
                        .InitialPushConstants = std::as_bytes(std::span{ &initial, size_t{ 1 }}),
                    });
                    break;
                }

                case EParticleRenderMode::Mesh:
                {
                    constexpr SMeshPushConstants initial{};

                    batch.PreparedMaterial = m_MaterialSystem->PrepareMaterialPass({
                        .Pass = EMaterialPass::ParticleMesh,
                        .Material = material,
                        .Pipeline = {
                            .VertexLayoutKey = uint64_t(runtime->Key),
                            .VertexLayout = &runtime->MeshVertexLayout,
                        },
                        .ExternalResources = {
                            .ConstantBuffers = constantBuffers,
                        },
                        .InitialPushConstants = std::as_bytes(std::span{ &initial, size_t{ 1 }}),
                    });
                    break;
                }
            }
        }
    }

    Renderer::SMaterialFrameInputs Renderer::CollectMaterialFrameInputs(
        const std::vector<SSubmittedSystemInstance>& instances
    ) const
    {
        SMaterialFrameInputs inputs;

        for (const auto& instance : instances)
        {
            const auto& emitters = instance.Instance->GetCompiledSystem().Emitters;

            for (const auto& emitter : emitters)
            {
                if (emitter.MaxParticles == 0)
                    continue;

                EMaterialPass pass;
                if (!TryToGetParticleMaterialPass(emitter.RenderMode, pass))
                    continue;

                const auto& material = m_MaterialSystem->ResolveParticleMaterial(
                    pass,
                    emitter.Material
                );

                inputs.Materials.push_back(material);

                if (emitter.RenderMode == EParticleRenderMode::Sprite && emitter.SpriteTexture)
                    inputs.Textures.push_back(emitter.SpriteTexture);
            }
        }

        return inputs;
    }

    void Renderer::SimulateBatch(const Ref<CommandBuffer>& cmd, const SSimulationBatch& batch)
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
            const auto& system = instance->Instance->GetCompiledSystem();
            const auto emitterCount = instance->Allocation.Emitters.Count;

            m_LastSubmissionMetrics.ScheduledEmitterCount += emitterCount;

            for (uint32_t i = 0; i < emitterCount; ++i)
            {
                const auto maxParticles = system.Emitters[i].MaxParticles;
                if (maxParticles == 0) continue;

                ++m_LastSubmissionMetrics.SpawnDispatchCount;

                const SSpawnPushConstants pc
                {
                    .InstanceIndex = instance->Allocation.InstanceIndex,
                    .EmitterIndex = i,
                };

                runtime->SpawnShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
                cmd->Dispatch((maxParticles + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
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
            cmd->Dispatch((instance->Allocation.Particles.Count + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }
    }

    void Renderer::RenderBatch(const Ref<CommandBuffer>& cmd, const SRenderBatch& batch)
    {
        if (!batch.PreparedMaterial)
            return;

        const auto* runtime = FindParticleStateLayoutRuntime(batch.Key.ParticleStateLayout);
        EE_CORE_ASSERT(runtime, "Aether particle state layout runtime is missing.")
        if (!runtime) return;

        const auto& particleBuffer = runtime->ParticleStateBuffer;

        switch (batch.Key.RenderMode)
        {
            case EParticleRenderMode::Sprite:
            {
                const auto drawSprite = [
                    &batch,
                    &particleBuffer
                ](const Ref<CommandBuffer>& cmd, const Ref<Shader>& shader)
                {
                    particleBuffer->BindAs<VertexBuffer>(cmd);

                    for (const auto& item : batch.Items)
                    {
                        const auto worldTransform = GetParticleRenderTransform(
                            *item.Emitter,
                            *item.Instance->Instance
                        );

                        const SSpritePushConstants pc{
                            .WorldTransform = worldTransform,
                            .MaterialIndex = item.MaterialIndex,
                            .SpriteIndex = item.SpriteIndex,
                        };

                        shader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
                        cmd->Draw(
                            6,
                            item.Emitter->MaxParticles,
                            0,
                            item.Instance->Allocation.Particles.Offset + item.Emitter->LocalParticleOffset
                        );
                    }
                };

                m_MaterialSystem->DrawMaterial(
                    {
                        .CommandBuffer = cmd,
                        .MaterialPass = &*batch.PreparedMaterial,
                    },
                    drawSprite
                );

                return;
            }

            case EParticleRenderMode::Ribbon:
            {
                const auto drawRibbon = [
                    &batch
                ](const Ref<CommandBuffer>& cmd, const Ref<Shader>& shader)
                {
                    for (const auto& item : batch.Items)
                    {
                        const auto worldTransform = GetParticleRenderTransform(
                            *item.Emitter,
                            *item.Instance->Instance
                        );

                        const SRibbonPushConstants pc{
                            .WorldTransform = worldTransform,
                            .EmitterIndex = item.Instance->Allocation.Emitters.Offset + item.LocalEmitterIndex,
                            .ParticleBaseOffset = item.Instance->Allocation.Particles.Offset,
                            .MaterialIndex = item.MaterialIndex,
                        };

                        shader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
                        cmd->Draw(item.Emitter->MaxParticles * 6);
                    }
                };

                m_MaterialSystem->DrawMaterial(
                    {
                        .CommandBuffer = cmd,
                        .MaterialPass = &*batch.PreparedMaterial,
                    },
                    drawRibbon
                );

                return;
            }

            case EParticleRenderMode::Mesh:
            {
                const auto drawMesh = [
                    this,
                    &batch,
                    &particleBuffer
                ](const Ref<CommandBuffer>& cmd, const Ref<Shader>& shader)
                {
                    m_MeshVertexBuffer->Bind(cmd);

                    // TODO: Enhance this api
                    particleBuffer->BindAs<VertexBuffer>(cmd, std::span<uint64_t>{}, 1, 1);

                    for (const auto& item : batch.Items)
                    {
                        const auto worldTransform = GetParticleRenderTransform(
                            *item.Emitter,
                            *item.Instance->Instance
                        );

                        const SMeshPushConstants pc{
                            .WorldTransform = worldTransform,
                            .MaterialIndex = item.MaterialIndex
                        };

                        shader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(pc));
                        cmd->Draw(
                            m_MeshVertexCount,
                            item.Emitter->MaxParticles,
                            0,
                            item.Instance->Allocation.Particles.Offset + item.Emitter->LocalParticleOffset
                        );
                    }
                };

                m_MaterialSystem->DrawMaterial(
                    {
                        .CommandBuffer = cmd,
                        .MaterialPass = &*batch.PreparedMaterial,
                    },
                    drawMesh
                );

                return;
            }
        }

        EE_CORE_ERROR(
            "Aether cannot render unknown particle render mode '{}'.",
            (uint32_t)batch.Key.RenderMode
        )
    }

    void Renderer::BarrierSchedulingBuffers(const Ref<CommandBuffer>& cmd) const
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

    void Renderer::ClearParticleAllocation(const SSystemInstanceAllocation& allocation)
    {
        const auto* layout = m_ParticleStateLayouts.Find(allocation.ParticleStateLayout);
        const auto* runtime = FindParticleStateLayoutRuntime(allocation.ParticleStateLayout);

        runtime->ParticleStateBuffer->Fill(
            0,
            int32_t(allocation.Particles.Offset * layout->ParticleStateStride),
            allocation.Particles.Count * layout->ParticleStateStride
        );
    }
}
