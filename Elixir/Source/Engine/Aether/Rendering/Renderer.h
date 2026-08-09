#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Aether/System.h>
#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Core/ResourcePool.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>

namespace Elixir
{
    class MaterialSystem;
    class MaterialRenderScene;
}

namespace Elixir::Aether::Rendering
{
    using namespace Elixir;
    using namespace Elixir::Aether;
    using namespace Elixir::Aether::Core;

    struct alignas(16) SFrameData
    {
        glm::mat4 View;
        glm::mat4 Proj;
        glm::mat4 ViewProj;
        glm::vec3 CameraPos;
        float     Time  = 0.0f;
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
        size_t SubmittedMaterialCount = 0;
    };

    /**
     * @brief Simulates and renders Aether system instances on the GPU.
     *
     * Renderer consumes immutable SystemInstanceRenderProxy objects from a
     * FrameSubmission. It allocates the shared logical ranges required by compiled
     * systems, uploads changed system and parameter data, runs particle simulation,
     * and submits geometry to MaterialSystem.
     *
     * Renderer owns Aether GPU resource lifetime. It defers released allocations
     * until the frame slot that used them has completed its GPU work.
     *
     * Renderer does not inspect mutable SystemInstance state and does not author or
     * resolve materials. It builds renderer-owned geometry data and delegates
     * material drawing to MaterialSystem.
     *
     * @thread_safety Render-thread confined. Call all public methods from the
     * render-frame path.
     */
    class ELIXIR_API Renderer final
    {
      public:
        /** Number of threads used by Aether compute shader dispatches. */
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        /**
         * @brief Creates the GPU renderer for Aether system instances.
         *
         * @param context Graphics context that owns frame synchronization and GPU
         * resources.
         * @param shaderLoader Loader used to obtain Aether compute shaders.
         * @param materialSystem Application material system used to render particle
         * geometry.
         * @param limits Logical capacities for shared Aether resource tables.
         *
         * @pre context is not null.
         * @pre shaderLoader is not null.
         * @pre context, shaderLoader, and materialSystem outlive this renderer.
         */
        Renderer(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            MaterialSystem& materialSystem,
            const SResourcePoolLimits& limits = {}
        );

        /**
         * @brief Advances renderer frame state.
         *
         * Updates frame timing data and processes allocations whose deferred GPU
         * retirement is now safe.
         *
         * @param timestep Elapsed time for the current frame.
         *
         * @pre Call once per render frame before Render().
         */
        void Update(const Timestep& timestep);

        /**
         * @brief Simulates and renders a published Aether frame submission.
         *
         * The method resolves per-instance GPU allocations, uploads data whose
         * revision changed, executes particle compute passes, and submits the
         * resulting geometry through MaterialSystem.
         *
         * @param submission Immutable instance proxies to simulate and render.
         * @param camera Camera used to build particle render data.
         *
         * @pre Update() was called for the current frame.
         */
        void Render(const FrameSubmission& submission, const Camera& camera);

        /**
         * @brief Retires the GPU allocation owned by one detached system instance.
         *
         * The allocation is removed from active renderer records immediately and is
         * released to ResourcePool only after the applicable frame fence completes.
         *
         * @param key Internal identity of the detached system instance.
         *
         * @note Calling this method for an unknown key has no effect.
         */
        void Retire(const SSystemInstanceKey& key);

        /**
         * @brief Returns metrics collected for the most recent rendered submission.
         *
         * @return Read-only frame metrics.
         *
         * @note Read the result after Render() completes at a frame boundary.
         */
        const SParticleSubmissionMetrics& GetLastSubmissionMetrics() const;

      private:
        // Owns GPU resources and compute pipelines for one particle-state layout.
        struct SParticleStateLayoutRuntime
        {
            EParticleStateLayout Key = EParticleStateLayout::CoreV1;
            Ref<StorageBuffer> ParticleStateBuffer;

            Ref<Shader> SpawnShader;
            Ref<ComputePipeline> SpawnPipeline;
            Ref<Shader> UpdateShader;
            Ref<ComputePipeline> UpdatePipeline;

            // Geometry ABI owned by Particles System. MaterialRenderer receives these
            // layouts to create the pipeline for the selected material pass.
            BufferLayout SpriteVertexLayout;
            BufferLayout MeshVertexLayout;

            bool IsReady() const
            {
                return ParticleStateBuffer &&
                    SpawnShader && SpawnPipeline &&
                    UpdateShader && UpdatePipeline;
            }
        };

        // Initializes Aether shaders, pipelines, layouts, and shared GPU buffers.
        void Init(const ShaderLoader* shaderLoader);

        // Create the GPU runtime for the built-in CoreV1 particle-state layout.
        void CreateCoreV1ParticleStateLayoutRuntime(const ShaderLoader* shaderLoader);

        // Creates shared GPU buffers sized from the configured resource-pool limits.
        void CreateBuffers();

        // Creates the unit mesh geometry used by mesh particle rendering.
        void CreateMeshVertexBuffer();

        // Initializes per-frame constant-buffer data.
        void InitPerFrameData();

        // Binds shared Aether buffers to scheduler and simulation shaders.
        void BindShaderParameters();

        // Binds buffers and constants specific to one particle-state layout.
        void BindParticleStateLayoutShaderParameters(const SParticleStateLayoutRuntime& runtime) const;

        // Begins the graphics rendering scope for particle material passes.
        void BeginRendering(const Ref<CommandBuffer>& cmd) const;

        // Ends the graphics rendering scope for particle material passes.
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        // Tracks the GPU allocation and uploaded revisions for one live system instance.
        struct SInstanceRecord
        {
            SSystemInstanceKey SystemInstanceKey;
            uint32_t SystemInstanceRevision = 0;
            UUID CompiledSystemId;
            uint32_t CompilationRevision = 0;
            uint32_t ParameterRevision = 0;
            SSystemInstanceAllocation Allocation;
        };

        // Pairs one immutable render proxy with its renderer-owned GPU allocation.
        struct SSubmittedSystemInstance
        {
            Ref<const SystemInstanceRenderProxy> Proxy;
            SSystemInstanceAllocation Allocation;
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
        };

        // Groups submitted instances that use the same particle-state layout.
        struct SSimulationBatch
        {
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
            std::vector<const SSubmittedSystemInstance*> Instances;
        };

        // Finds or creates the renderer record required by an immutable instance proxy.
        SInstanceRecord* ResolveInstanceRecord(const SystemInstanceRenderProxy& proxy);

        // Uploads compiled emitter, operation, trigger, and system data for an allocation.
        void UploadCompiledSystem(
            const SystemInstanceRenderProxy& proxy,
            const SSystemInstanceAllocation& allocation
        ) const;

        // Uploads resolved instance parameter values for an allocation.
        void UploadInstanceParameters(
            const SystemInstanceRenderProxy& proxy,
            const SSystemInstanceAllocation& allocation
        ) const;

        // Defers an allocation release until its frame slot is safe to recycle.
        void QueueRetirement(SSystemInstanceAllocation allocation);

        // Returns allocations whose associated GPU work has completed to ResourcePool.
        void ProcessCompletedRetirements();

        // Updates GPU tables when the proxy revisions differ from the instance record.
        void UpdateBuffers(const SystemInstanceRenderProxy& proxy, SInstanceRecord& record);

        // Finds the mutable GPU runtime for a registered particle-state layout.
        SParticleStateLayoutRuntime* FindParticleStateLayoutRuntime(EParticleStateLayout layout);

        // Finds the read-only GPU runtime for a registered particle-state layout.
        const SParticleStateLayoutRuntime* FindParticleStateLayoutRuntime(EParticleStateLayout layout) const;

        // Returns whether the renderer has a ready GPU runtime for the layout.
        bool IsParticleStateLayoutSupported(EParticleStateLayout layout) const;

        // Groups submitted instances into simulation batches by particle-state layout.
        std::vector<SSimulationBatch> BuildSimulationBatches(
            const std::vector<SSubmittedSystemInstance>& instances
        ) const;

        // Build material render items from submitted particle instances.
        MaterialRenderScene BuildMaterialRenderScene(
            const std::vector<SSubmittedSystemInstance>& instances
        ) const;

        // Dispatch GPU simulation passes for all instances in one layout batch.
        void SimulateBatch(
            const Ref<CommandBuffer>& cmd,
            const SSimulationBatch& batch
        );

        // Makes scheduler writes visibility to subsequent Aether compute passes.
        void BarrierSchedulingBuffers(const Ref<CommandBuffer>& cmd) const;

        // Clears persistent particle state before a released allocation is reused.
        void ClearParticleAllocation(const SSystemInstanceAllocation& allocation);

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        // Mirrors one CoreV1 particle state in GPU storage.
        struct alignas(16) SGPUParticleState
        {
            glm::vec4 PositionSize{};
            glm::vec4 VelocityAge{};
            glm::vec4 Transform{};
            glm::vec4 TangentRibbonId{};
            glm::vec4 Color{};
            glm::vec4 Metadata{};
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

        // Update dispatches use the same ABI as scheduler dispatches.
        using SUpdatePushConstants = SSchedulePushConstants;

        Ref<Shader> m_SchedulerBeginShader;
        Ref<ComputePipeline> m_SchedulerBeginPipeline;
        Ref<Shader> m_SchedulerInitEmittersShader;
        Ref<ComputePipeline> m_SchedulerInitEmittersPipeline;
        Ref<Shader> m_SchedulerScheduleEmittersShader;
        Ref<ComputePipeline> m_SchedulerScheduleEmittersPipeline;
        Ref<Shader> m_SchedulerFinalizeShader;
        Ref<ComputePipeline> m_SchedulerFinalizePipeline;

        SResourcePoolLimits m_ResourcePoolLimits;
        ParticleStateLayoutRegistry m_ParticleStateLayouts;
        std::vector<SParticleStateLayoutRuntime> m_ParticleStateLayoutRuntimes;
        ResourcePool m_ResourcePool;
        std::unordered_map<SSystemInstanceKey, SInstanceRecord> m_InstanceRecords;
        std::unordered_set<SSystemInstanceKey> m_AllocationFailures;
        std::unordered_set<SSystemInstanceKey> m_UnsupportedParticleStateLayoutInstances;
        std::array<
            std::vector<SSystemInstanceAllocation>,
            GraphicsContext::FRAMES
        > m_DeferredRetirements;

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

        MaterialSystem& m_MaterialSystem;

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
