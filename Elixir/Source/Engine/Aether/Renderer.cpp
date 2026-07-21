#include "epch.h"
#include "Renderer.h"

#include "Engine/Core/Color.h"
#include "Engine/Graphics/CommandBuffer.h"
#include "Engine/Graphics/SamplerBuilder.h"
#include "Engine/Graphics/Pipeline/PipelineBuilder.h"

namespace Elixir::Aether
{
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    struct SSpritePushConstants
    {
        uint32_t SpriteIndex = 0;
    };

    struct SRibbonPushConstants
    {
        uint32_t EmitterIndex = 0;
        uint32_t ParticleBaseOffset = 0;
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

    Renderer::Renderer(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        SParticlePoolLimits limits
    ) : m_ParticlePoolLimits(std::move(limits)),
        m_ParticleResourcePool(m_ParticlePoolLimits),
        m_GraphicsContext(context)
    {
        EE_CORE_INFO("Initializing Aether Renderer.")

        Init(shaderLoader);
        CreateBuffers();
        InitPerFrameData();
        BindShaderParameters();
    }

    void Renderer::Update(const Timestep& timestep)
    {
        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();
    }

    void Renderer::Render(const SystemInstance& instance, const Camera& camera)
    {
        const auto& system = instance.GetCompiledSystem();

        const auto* record = ResolveInstanceRecord(instance);
        if (!record) return;

        m_LastSubmissionMetrics = {
            .SubmissionSerial = ++m_SubmissionSerial,
            .DeltaTimeSeconds = m_LastDeltaTimeSeconds,
            .ElapsedTimeSeconds = m_ElapsedTimeSeconds,
            .RequestedEmitterCount =  system.Emitters.size(),
            .RequestedParticleCapacity = system.TotalMaxParticles
        };

        m_RenderExtent = m_GraphicsContext->GetRenderTarget()->GetExtent();

        m_FrameData.View = camera.GetViewMatrix();
        m_FrameData.Proj = camera.GetProjectionMatrix();
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameData.CameraPos = camera.GetPosition();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(SFrameData));

        const auto emitterCount = record->Allocation.Emitters.Count;
        const auto maxParticles = record->Allocation.Particles.Count;

        m_LastSubmissionMetrics.SubmittedEmitterCount = emitterCount;
        m_LastSubmissionMetrics.SubmittedParticleCapacity = maxParticles;
        m_LastSubmissionMetrics.ScheduledEmitterCount = emitterCount;
        m_LastSubmissionMetrics.TriggerEventCapacityPerEmitter =
            m_ParticlePoolLimits.TriggerEventCapacityPerEmitter;

        UpdateBuffers(instance, *record);

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();
        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment = m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_RenderExtent
        });

        // Scheduling

        const SSchedulePushConstants schedulePushConstants{
            .InstanceIndex = record->Allocation.InstanceIndex,
        };

        m_SchedulerBeginPipeline->Bind(cmd);
        m_SchedulerBeginShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch(1);

        BarrierSchedulingBuffers(cmd);

        m_SchedulerInitEmittersPipeline->Bind(cmd);
        m_SchedulerInitEmittersShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch((emitterCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        BarrierSchedulingBuffers(cmd);

        m_SchedulerScheduleEmittersPipeline->Bind(cmd);
        m_SchedulerScheduleEmittersShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch((emitterCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        BarrierSchedulingBuffers(cmd);

        m_SchedulerFinalizePipeline->Bind(cmd);
        m_SchedulerFinalizeShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&schedulePushConstants,
            sizeof(schedulePushConstants)
        );
        cmd->Dispatch(1);

        BarrierSchedulingBuffers(cmd);

        // Spawning

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        m_SpawnPipeline->Bind(cmd);
        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto particleCount = system.Emitters[i].MaxParticles;
            if (particleCount == 0) continue;

            ++m_LastSubmissionMetrics.SpawnDispatchCount;

            const SSpawnPushConstants spawnPushConstants
            {
                .InstanceIndex = record->Allocation.InstanceIndex,
                .EmitterIndex = i,
            };

            m_SpawnShader->SetPushConstant(
                cmd,
                "pc",
                (void*)&spawnPushConstants,
                sizeof(SSpawnPushConstants)
            );

            cmd->Dispatch((particleCount + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }

        // Updating

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader,
            EPipelineAccess::ShaderRead | EPipelineAccess::ShaderWrite
        );

        m_UpdatePipeline->Bind(cmd);

        const SUpdatePushConstants updatePushConstants
        {
            .InstanceIndex = record->Allocation.InstanceIndex,
        };

        m_UpdateShader->SetPushConstant(
            cmd,
            "pc",
            (void*)&updatePushConstants,
            sizeof(SUpdatePushConstants)
        );

        cmd->Dispatch((maxParticles + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);

        // Rendering

        m_ParticleBuffer->Barrier(
            cmd,
            EPipelineStage::VertexShader | EPipelineStage::VertexInput,
            EPipelineAccess::ShaderRead | EPipelineAccess::VertexAttributeRead
        );

        BeginRendering(cmd);

        bool spritePipelineBound = false;
        bool ribbonPipelineBound = false;

        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];

            if (emitter.RenderMode != EParticleRenderMode::Mesh || emitter.MaxParticles == 0)
                continue;

            m_MeshPipeline->Bind(cmd);
            m_MeshVertexBuffer->Bind(cmd);
            // TODO: Enhance this api
            m_ParticleBuffer->BindAs<VertexBuffer>(cmd, std::span<uint64_t>{}, 1, 1);

            cmd->Draw(
                m_MeshVertexCount,
                emitter.MaxParticles,
                0,
                record->Allocation.Particles.Offset + emitter.LocalParticleOffset
            );
        }

        for (uint32_t i = 0; i < emitterCount; ++i)
        {
            const auto& emitter = system.Emitters[i];

            if (emitter.RenderMode == EParticleRenderMode::Mesh || emitter.MaxParticles == 0)
                continue;

            if (emitter.RenderMode == EParticleRenderMode::Ribbon)
            {
                if (!ribbonPipelineBound)
                {
                    m_RibbonPipeline->Bind(cmd);
                    ribbonPipelineBound = true;
                    spritePipelineBound = false;
                }

                const SRibbonPushConstants pc{
                    .EmitterIndex = record->Allocation.Emitters.Offset + i,
                    .ParticleBaseOffset = record->Allocation.Particles.Offset,
                };
                m_RibbonShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(SRibbonPushConstants));

                cmd->Draw(emitter.MaxParticles * 6);
                continue;
            }

            if (!spritePipelineBound)
            {
                m_SpritePipeline->Bind(cmd);
                m_ParticleBuffer->BindAs<VertexBuffer>(cmd);
                spritePipelineBound = true;
                ribbonPipelineBound = false;
            }

            const SSpritePushConstants pc{ ResolveSpriteIndex(emitter.SpriteTexture) };
            m_SpriteShader->SetPushConstant(cmd, "pc", (void*)&pc, sizeof(SSpritePushConstants));

            cmd->Draw(
                6,
                emitter.MaxParticles,
                0,
                record->Allocation.Particles.Offset + emitter.LocalParticleOffset
            );
        }

        EndRendering(cmd);
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

        m_SpawnShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesSpawn" },
            "ParticlesSpawn",
            EShaderStage::Compute
        );

        m_UpdateShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "ParticlesUpdate" },
            "ParticlesUpdate",
            EShaderStage::Compute
        );

        m_SpriteShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Sprite" },
            "SpriteRenderer"
        );

        m_RibbonShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Ribbon" },
            "RibbonRenderer"
        );

        m_MeshShader = shaderLoader->LoadShader(
            "./Shaders/Aether/",
            std::array<std::string_view, 1>{ "Mesh" },
            "MeshRenderer"
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

        pipelineInfo.Shader = m_SpawnShader;
        m_SpawnPipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        pipelineInfo.Shader = m_UpdateShader;
        m_UpdatePipeline = ComputePipeline::Create(m_GraphicsContext, pipelineInfo);

        const BufferLayout spriteBufferLayout({
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
        });

        PipelineBuilder spriteBuilder;
        spriteBuilder.SetShader(m_SpriteShader);
        spriteBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        spriteBuilder.SetPolygonMode(EPolygonMode::Fill);
        spriteBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        spriteBuilder.EnableAlphaBlending();
        spriteBuilder.DisableDepthTest();
        spriteBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        spriteBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        spriteBuilder.SetBufferLayout(spriteBufferLayout);
        m_SpritePipeline = spriteBuilder.Build(m_GraphicsContext);

        m_Sprites = TextureSet::Create(m_GraphicsContext);
        m_SpriteSampler = SamplerBuilder().Build(m_GraphicsContext);

        PipelineBuilder ribbonBuilder;
        ribbonBuilder.SetShader(m_RibbonShader);
        ribbonBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        ribbonBuilder.SetPolygonMode(EPolygonMode::Fill);
        ribbonBuilder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        ribbonBuilder.EnableAlphaBlendingMax();
        ribbonBuilder.DisableDepthTest();
        ribbonBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        ribbonBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        ribbonBuilder.SetBufferLayout({});
        m_RibbonPipeline = ribbonBuilder.Build(m_GraphicsContext);

        const BufferLayout meshBufferLayout({
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
        });

        PipelineBuilder meshBuilder;
        meshBuilder.SetShader(m_MeshShader);
        meshBuilder.SetInputTopology(EPrimitiveTopology::TriangleList);
        meshBuilder.SetPolygonMode(EPolygonMode::Fill);
        meshBuilder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
        meshBuilder.EnableAlphaBlendingMax();
        meshBuilder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        meshBuilder.SetDepthAttachmentFormat(EDepthStencilImageFormat::D32_SFLOAT);
        meshBuilder.SetBufferLayout(meshBufferLayout);

        auto info = meshBuilder.GetCreateInfo();
        info.DepthStencil.DepthTestEnable = true;
        info.DepthStencil.DepthWriteEnable = true;
        info.DepthStencil.DepthCompareOp = ECompareOp::LessOrEqual;

        m_MeshPipeline = GraphicsPipeline::Create(m_GraphicsContext, info);
    }

    void Renderer::CreateBuffers()
    {
        m_ParticleBuffer = StorageBuffer::Create(
            m_GraphicsContext,
            sizeof(SGPUParticleState) * m_ParticlePoolLimits.ParticleCapacity
        );

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

        m_MeshVertexBuffer->SetLayout(m_MeshPipeline->GetBufferLayout());
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
        constexpr SSchedulePushConstants schedulePushConstants{ .InstanceIndex = 0 };

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

        constexpr SSpawnPushConstants spawnPushConstants
        {
            .InstanceIndex = 0,
            .EmitterIndex = 0
        };

        m_SpawnShader->SetPushConstant(
            "pc",
            (void*)&spawnPushConstants,
            sizeof(spawnPushConstants)
        );

        m_SpawnShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_SpawnShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_SpawnShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_SpawnShader->BindStorageBuffer("spawnRequests", m_SpawnRequestBuffer);
        m_SpawnShader->BindStorageBuffer("ops", m_OpBuffer);
        m_SpawnShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_SpawnShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        constexpr SUpdatePushConstants updatePushConstants{ .InstanceIndex = 0 };
        m_UpdateShader->SetPushConstant(
            "pc",
            (void*)&updatePushConstants,
            sizeof(updatePushConstants)
        );

        m_UpdateShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_UpdateShader->BindStorageBuffer("instances", m_SystemInstanceBuffer);
        m_UpdateShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_UpdateShader->BindStorageBuffer("ops", m_OpBuffer);
        m_UpdateShader->BindStorageBuffer("parameters", m_ParameterBuffer);
        m_UpdateShader->BindConstantBuffer("cbParams", m_ParamsBuffer);

        const auto whiteTex = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_SRGB,
            1, 1,
            &Color::WhiteAlpha
        );

        m_WhiteTextureHandle = m_Sprites->AddTexture(whiteTex);

        const SSpritePushConstants spritePushConstants{ m_WhiteTextureHandle.Index };
        m_SpriteShader->SetPushConstant(
            "pc",
            (void*)&spritePushConstants,
            sizeof(spritePushConstants)
        );

        m_SpriteShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
        m_SpriteShader->BindTextureSet("sprites", m_Sprites);
        m_SpriteShader->BindSampler("spriteSampler", m_SpriteSampler);

        constexpr SRibbonPushConstants ribbonPushConstants{ 0 };
        m_RibbonShader->SetPushConstant(
            "pc",
            (void*)&ribbonPushConstants,
            sizeof(ribbonPushConstants)
        );

        m_RibbonShader->BindStorageBuffer("particles", m_ParticleBuffer);
        m_RibbonShader->BindStorageBuffer("emitters", m_EmitterBuffer);
        m_RibbonShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);

        m_MeshShader->BindConstantBuffer("cbFrame", m_FrameConstantBuffer);
    }

    uint32_t Renderer::ResolveSpriteIndex(const Ref<Texture2D>& texture)
    {
        if (!texture)
            return m_WhiteTextureHandle.Index;

        if (const auto it = m_SpriteTextures.find(texture); it != m_SpriteTextures.end())
            return it->second.Index;

        const auto handle = m_Sprites->AddTexture(texture);
        m_SpriteTextures[texture] = handle;

        return handle.Index;
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
        const auto found = m_InstanceRecords.find(instance.GetId());
        if (found != m_InstanceRecords.end())
        {
            return &found->second;
        }

        const auto& system = instance.GetCompiledSystem();
        const auto allocation = m_ParticleResourcePool.Allocate(system);

        if (!allocation)
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

        const auto [it, inserted] = m_InstanceRecords.emplace(
            instance.GetId(),
            SInstanceRecord{
                .SystemInstanceId = instance.GetId(),
                .CompiledSystemId = system.SourceId,
                .CompilationRevision = system.CompilationRevision,
                .Allocation = *allocation,
            }
        );

        EE_CORE_ASSERT(inserted, "Aether system instance registry insertion failed.")

        UploadCompiledSystem(system, it->second.Allocation);

        m_AllocationFailures.erase(instance.GetId());
        return &it->second;
    }

    void Renderer::UpdateBuffers(const SystemInstance& instance, const SInstanceRecord& record)
    {
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

    void Renderer::UploadCompiledSystem(
        const SCompiledSystem& system,
        const SSystemInstanceAllocation& allocation
    ) const
    {
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

        auto* parameters = (SParameterData*)m_ParameterBuffer->Map();
        for (uint32_t i = 0; i < allocation.Parameters.Count; ++i)
            parameters[allocation.Parameters.Offset + i] =
                ToParameterDescription(system.Parameters[i]);

        auto* targets = (STriggerTargetData*)m_TriggerTargetBuffer->Map();
        for (uint32_t i = 0; i < allocation.TriggerTargets.Count; ++i)
            targets[allocation.TriggerTargets.Offset + i] =
                ToTriggerTargetDescription(system.TriggerTargets[i]);
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
}
