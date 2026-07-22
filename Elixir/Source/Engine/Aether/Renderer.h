#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Aether/System.h>
#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/ParticleResourcePool.h>
#include <Engine/Aether/ParticleStateLayout.h>
#include <Engine/Aether/FrameSubmission.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::Aether
{
    struct alignas(16) SFrameData
    {
        glm::mat4 View;
        glm::mat4 Proj;
        glm::mat4 ViewProj;
        glm::vec3 CameraPos;
    };

    struct alignas(16) SParamsData
    {
        glm::vec4 Time{};
        glm::vec4 Viewport{};
    };

    struct SEmitterData
    {
        glm::vec4 MetaA{};
        glm::vec4 MetaB{};
        glm::vec4 MetaC{};
        glm::vec4 MetaD{};
    };

    struct SParticleOpData
    {
        glm::vec4 Header{};
        glm::vec4 Data0{};
        glm::vec4 Data1{};
        glm::vec4 Data2{};
    };

    struct SParameterData
    {
        glm::vec4 Value{};
    };

    struct SSystemInstanceData
    {
        uint32_t ParticleBaseOffset = 0;
        uint32_t EmitterBaseOffset = 0;
        uint32_t OpBaseOffset = 0;
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

    struct SEmitterInstanceStateData
    {
        float SpawnAccumulator = 0.0f;
        float BurstAccumulator = 0.0f;
        uint32_t BufferCursor = 0;
        uint32_t EmissionIndex = 0;

        uint32_t Generation = 0;
    };

    struct SSpawnRequestData
    {
        uint32_t SpawnCursor = 0;
        uint32_t SpawnCount = 0;
        uint32_t EmissionIndex = 0;
        uint32_t Generation = 0;
    };

    struct STriggerTargetData
    {
        uint32_t TargetEmitterIndex = 0;
        uint32_t BurstCount = 0;
        float DelaySeconds = 0.0f;
    };

    struct STriggerEventData
    {
        float RemainingDelaySeconds = 0.0f;
        uint32_t SpawnCount = 0;
        uint32_t Generation = 0;
    };

    struct STriggerQueueStateData
    {
        uint32_t Count = 0;
        uint32_t OverflowCount = 0;
    };

    struct SSystemSchedulerStateData
    {
        uint32_t Generation = 0;
        uint32_t ActiveTriggerBufferIndex = 0;
        uint32_t ResetPending = 0;
    };

    // CPU-side observability only. These values describe the complete particle
    // frame submitted for one Render() call; they do not read back GPU state.
    struct SParticleSubmissionMetrics
    {
        uint64_t SubmissionSerial = 0u;
        float DeltaTimeSeconds = 0.0f;
        float ElapsedTimeSeconds = 0.0f;

        size_t RequestedSystemInstanceCount = 0;
        size_t SubmittedSystemInstanceCount = 0;

        size_t RequestedEmitterCount = 0u;
        size_t SubmittedEmitterCount = 0u;

        uint32_t RequestedParticleCapacity = 0u;
        uint32_t SubmittedParticleCapacity = 0u;

        uint32_t ScheduledEmitterCount = 0;
        uint32_t SpawnDispatchCount = 0;
        uint32_t TriggerEventCapacityPerEmitter = 0;

        size_t SimulationBatchCount = 0;
        size_t RenderBatchCount = 0;
        size_t SubmittedRenderItemCount = 0;
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        Renderer(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const SParticlePoolLimits& limits = {}
        );

        void Update(const Timestep& timestep);
        void Render(const FrameSubmission& submission, const Camera& camera);

        // Must be called from the render-frame callback. The allocation remains
        // resident until the current frame slot is recycled after its GPU fence.
        void Retire(const SystemInstance& instance);

        // Read only at the frame boundary after Render() returns.
        const SParticleSubmissionMetrics& GetLastSubmissionMetrics() const;

      private:
        void Init(const ShaderLoader* shaderLoader);
        void CreateBuffers();
        void CreateMeshVertexBuffer();
        void InitPerFrameData();
        void BindShaderParameters();

        uint32_t ResolveSpriteIndex(const Ref<Texture2D>& texture);

        void BeginRendering(const Ref<CommandBuffer>& cmd) const;
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        struct SInstanceRecord
        {
            UUID SystemInstanceId;
            uint32_t SystemInstanceRevision = 0;
            UUID CompiledSystemId;
            uint32_t CompilationRevision = 0;
            SSystemInstanceAllocation Allocation;
        };

        struct SParticleStateArena
        {
            EParticleStateLayout Key = EParticleStateLayout::CoreV1;
            Ref<StorageBuffer> ParticleBuffer;
        };

        // Frame-local, renderer-owned snapshot. It decouples batch execution
        // from m_InstanceRecords and remains valid for the complete Render().
        struct SSubmittedSystemInstance
        {
            const SystemInstance* Instance = nullptr;
            SSystemInstanceAllocation Allocation;
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
        };

        struct SSimulationBatch
        {
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
            std::vector<const SSubmittedSystemInstance*> Instances;
        };

        struct SRenderBatchKey
        {
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
            EParticleRenderMode RenderMode = EParticleRenderMode::Sprite;

            bool operator==(const SRenderBatchKey&) const = default;
        };

        struct SRenderItem
        {
            const SSubmittedSystemInstance* Instance = nullptr;
            const SCompiledEmitter* Emitter = nullptr;
            uint32_t LocalEmitterIndex = 0;
        };

        struct SRenderBatch
        {
            SRenderBatchKey Key;
            std::vector<SRenderItem> Items;
        };

        SInstanceRecord* ResolveInstanceRecord(const SystemInstance& instance);
        void UploadCompiledSystem(
            const SCompiledSystem& system,
            const SSystemInstanceAllocation& allocation
        ) const;

        void QueueRetirement(SSystemInstanceAllocation allocation);
        void ProcessCompletedRetirements();

        void UpdateBuffers(const SystemInstance& instance, const SInstanceRecord& record);

        SParticleStateArena* FindParticleStateArena(EParticleStateLayout layout);
        const SParticleStateArena* FindParticleStateArena(EParticleStateLayout layout) const;

        bool IsParticleStateLayoutSupported(EParticleStateLayout layout) const;

        std::vector<SSimulationBatch>
        BuildSimulationBatches(const std::vector<SSubmittedSystemInstance>& instances) const;

        std::vector<SRenderBatch>
        BuildRenderBatches(const std::vector<SSubmittedSystemInstance>& instances) const;

        void SimulateBatch(
            const Ref<CommandBuffer>& cmd,
            const SSimulationBatch& batch
        );

        void RenderBatch(
            const Ref<CommandBuffer>& cmd,
            const SRenderBatch& batch
        );

        void BarrierSchedulingBuffers(const Ref<CommandBuffer>& cmd) const;

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        struct alignas(16) SGPUParticleState
        {
            glm::vec4 PositionSize{};
            glm::vec4 VelocityAge{};
            glm::vec4 Transform{};
            glm::vec4 TangentRibbonId{};
            glm::vec4 Color{};
            glm::vec4 Metadata{};
        };

        struct SSchedulePushConstants
        {
            uint32_t InstanceIndex = 0;
        };

        struct SSpawnPushConstants
        {
            uint32_t InstanceIndex = 0;
            uint32_t EmitterIndex = 0;
        };

        using SUpdatePushConstants = SSchedulePushConstants;

        Ref<Shader> m_SchedulerBeginShader;
        Ref<ComputePipeline> m_SchedulerBeginPipeline;
        Ref<Shader> m_SchedulerInitEmittersShader;
        Ref<ComputePipeline> m_SchedulerInitEmittersPipeline;
        Ref<Shader> m_SchedulerScheduleEmittersShader;
        Ref<ComputePipeline> m_SchedulerScheduleEmittersPipeline;
        Ref<Shader> m_SchedulerFinalizeShader;
        Ref<ComputePipeline> m_SchedulerFinalizePipeline;

        Ref<Shader> m_SpawnShader;
        Ref<ComputePipeline> m_SpawnPipeline;
        Ref<Shader> m_UpdateShader;
        Ref<ComputePipeline> m_UpdatePipeline;

        Ref<Shader> m_SpriteShader;
        Ref<GraphicsPipeline> m_SpritePipeline;
        Ref<Shader> m_RibbonShader;
        Ref<GraphicsPipeline> m_RibbonPipeline;
        Ref<Shader> m_MeshShader;
        Ref<GraphicsPipeline> m_MeshPipeline;

        SParticlePoolLimits m_ParticlePoolLimits;
        ParticleStateLayoutRegistry m_ParticleStateLayouts;
        ParticleResourcePool m_ParticleResourcePool;
        std::unordered_map<UUID, SInstanceRecord> m_InstanceRecords;
        std::unordered_set<UUID> m_AllocationFailures;
        std::unordered_set<UUID> m_UnsupportedParticleStateLayoutInstances;
        std::array<
            std::vector<SSystemInstanceAllocation>,
            GraphicsContext::FRAMES
        > m_DeferredRetirements;

        std::vector<SParticleStateArena> m_ParticleStateArenas;
        Ref<StorageBuffer> m_EmitterStateBuffer;
        Ref<StorageBuffer> m_SpawnRequestBuffer;
        Ref<DynamicStorageBuffer> m_TriggerTargetBuffer;
        std::array<Ref<StorageBuffer>, 2> m_TriggerEventBuffers;
        Ref<StorageBuffer> m_TriggerQueueStateBuffer;
        Ref<StorageBuffer> m_SystemSchedulerStateBuffer;

        Ref<DynamicStorageBuffer> m_SystemInstanceBuffer;
        Ref<DynamicStorageBuffer> m_EmitterBuffer;
        Ref<DynamicStorageBuffer> m_OpBuffer;
        Ref<DynamicStorageBuffer> m_ParameterBuffer;
        Ref<UniformBuffer> m_ParamsBuffer;

        Ref<TextureSet> m_Sprites;
        Ref<Sampler> m_SpriteSampler;
        std::unordered_map<Ref<Texture2D>, SResourceHandle> m_SpriteTextures;

        SResourceHandle m_WhiteTextureHandle{};

        uint32_t m_MeshVertexCount = 0;
        Ref<VertexBuffer> m_MeshVertexBuffer;

        float m_LastDeltaTimeSeconds = 0.0f;
        float m_ElapsedTimeSeconds = 0.0f;

        uint64_t m_SubmissionSerial = 0;
        SParticleSubmissionMetrics m_LastSubmissionMetrics{};

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}
