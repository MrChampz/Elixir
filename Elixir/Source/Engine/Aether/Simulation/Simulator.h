#pragma once

#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Core/ResourceAllocation.h>
#include <Engine/Aether/Simulation/RenderFrame.h>
#include <Engine/Aether/Simulation/ResourcePool.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>

namespace Elixir
{
    class CommandBuffer;
    class GraphicsContext;
    class ShaderLoader;
    class Timestep;
}

namespace Elixir::Aether::Simulation
{
    using namespace Core;

    /**
     * @brief Reports the simulation work for the latest frame submission.
     *
     * Requested values include all instances in the frame submission. Submitted
     * values include only instances that the simulator accepted. An instance may
     * be rejected when its particle layout is unsupported or its resources cannot
     * be allocated.
     *
     * Simulator replaces these values each time Simulate() runs.
     */
    struct SSimulationMetrics
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
        size_t   SimulationBatchCount = 0;

        uint32_t TriggerEventCapacityPerEmitter = 0;
    };

    /**
     * @brief Simulates Aether particle systems on the GPU.
     *
     * Simulator owns particle allocations, simulation buffers, compute pipelines,
     * and deferred resource retirement. It consumes immutable frame submissions and
     * produces immutable render frames.
     *
     * Simulator records simulation commands but does not submit command buffers or
     * render particle geometry.
     *
     * @thread_safety Use this class only from the render-frame thread.
     */
    class ELIXIR_API Simulator final
    {
    public:
        /** Number of threads used by Aether compute shader dispatches. */
        static constexpr uint32_t COMPUTE_GROUP_SIZE = 256;

        /**
         * @brief Creates the GPU resources required for particle simulation.
         *
         * @param context Graphics context that owns the GPU resources.
         * @param shaderLoader Loader used to create the simulation shaders.
         * @param limits Maximum capacity of each shared simulation resource.
         *
         * @pre context is not null and outlives the simulator.
         * @pre shaderLoader is not null.
         */
        Simulator(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const SResourcePoolLimits& limits = {}
        );

        /**
         * @brief Prepares the simulator for a new frame.
         *
         * This method releases allocations whose GPU work has completed. It also
         * updates the current and total simulation time.
         *
         * @param timestep Time elapsed since the previous frame.
         *
         * @note Call this method once per frame, after the graphics context prepares
         * the current frame slot and before calling Simulate().
         */
        void BeginFrame(const Timestep& timestep);

        /**
         * @brief Records particle simulation work for a frame submission.
         *
         * The method resolves instance allocations, uploads changed system data,
         * records compute work, and adds the barriers required for rendering.
         *
         * @param submission Immutable system instances requested for the frame.
         * @param cmd Command buffer that receives the simulation commands.
         * @return Immutable render data produced by the simulation.
         *
         * @pre cmd is not null and is recording commands.
         * @pre BeginFrame() was called for the current frame.
         */
        Ref<const RenderFrame> Simulate(
            const Rendering::FrameSubmission& submission,
            const Ref<CommandBuffer>& cmd
        );

        /**
         * @brief Stops tracking a system instance and schedules its allocation for release.
         *
         * The allocation remains valid until the GPU finishes the frame that last
         * used it. An unknown key has no effect.
         *
         * @param key Internal key of the system instance to retire.
         */
        void Retire(const SSystemInstanceKey& key);

        /**
         * @brief Returns metrics from the latest simulation.
         * @return Metrics produced by the latest call to Simulate().
         * @note A later call to Simulate() replaces the reported values.
         */
        const SSimulationMetrics& GetLastMetrics() const
        {
            return m_LastMetrics;
        }

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

            bool IsReady() const
            {
                return ParticleStateBuffer &&
                    SpawnShader && SpawnPipeline &&
                    UpdateShader && UpdatePipeline;
            }
        };

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

        // Pairs one immutable render proxy with its simulator-owned GPU allocation.
        struct SSubmittedSystemInstance
        {
            Ref<const Rendering::SystemInstanceRenderProxy> Proxy;
            SSystemInstanceAllocation Allocation;
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
        };

        // Groups submitted instances that use the same particle-state layout.
        struct SSimulationBatch
        {
            EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
            std::vector<const SSubmittedSystemInstance*> Instances;
        };

        // Initializes Aether shaders, pipelines, layouts, and shared GPU buffers.
        void Init(const ShaderLoader* shaderLoader);

        // Create the GPU runtime for the built-in CoreV1 particle-state layout.
        void CreateCoreV1ParticleStateLayoutRuntime(const ShaderLoader* shaderLoader);

        // Creates shared GPU buffers sized from the configured resource-pool limits.
        void CreateBuffers();

        // Binds shared Aether buffers to scheduler and simulation shaders.
        void BindShaderParameters();

        // Binds buffers and constants specific to one particle-state layout.
        void BindParticleStateLayoutShaderParameters(const SParticleStateLayoutRuntime& runtime) const;

        // Finds or creates the simulation record required by an immutable instance proxy.
        SInstanceRecord* ResolveInstanceRecord(const Rendering::SystemInstanceRenderProxy& proxy);

        // Updates GPU tables when the proxy revisions differ from the instance record.
        void UpdateBuffers(const Rendering::SystemInstanceRenderProxy& proxy, SInstanceRecord& record);

        // Uploads compiled emitter, operation, trigger, and system data for an allocation.
        void UploadCompiledSystem(
            const Rendering::SystemInstanceRenderProxy& proxy,
            const SSystemInstanceAllocation& allocation
        ) const;

        // Uploads resolved instance parameter values for an allocation.
        void UploadInstanceParameters(
            const Rendering::SystemInstanceRenderProxy& proxy,
            const SSystemInstanceAllocation& allocation
        ) const;

        // Finds the mutable GPU runtime for a registered particle-state layout.
        SParticleStateLayoutRuntime* FindParticleStateLayoutRuntime(EParticleStateLayout layout);

        // Finds the read-only GPU runtime for a registered particle-state layout.
        const SParticleStateLayoutRuntime* FindParticleStateLayoutRuntime(EParticleStateLayout layout) const;

        // Returns whether the simulator has a ready GPU runtime for the layout.
        bool IsParticleStateLayoutSupported(EParticleStateLayout layout) const;

        // Resolves allocations and removes instances that cannot be simulated.
        std::vector<SSubmittedSystemInstance> ResolveSubmittedInstances(
            const Rendering::FrameSubmission& submission
        );

        // Groups submitted instances into simulation batches by particle-state layout.
        static std::vector<SSimulationBatch> BuildSimulationBatches(
            const std::vector<SSubmittedSystemInstance>& instances
        );

        // Collects the particle-state buffers required for rendering.
        std::vector<SParticleStateRenderResource> BuildRenderResources() const;

        // Creates one render item for each renderable emitter.
        static std::vector<SRenderItem> BuildRenderItems(
            const std::vector<SSubmittedSystemInstance>& instances
        );

        // Dispatch GPU simulation passes for all instances in one layout batch.
        void SimulateBatch(const Ref<CommandBuffer>& cmd, const SSimulationBatch& batch);

        // Makes particle writes visible to the graphics pipeline.
        void PublishRenderBarrier(const Ref<CommandBuffer>& cmd, EParticleStateLayout layout) const;

        // Defers an allocation release until its frame slot is safe to recycle.
        void QueueRetirement(SSystemInstanceAllocation allocation);

        // Returns allocations whose associated GPU work has completed to ResourcePool.
        void ProcessCompletedRetirements();

        // Makes scheduler writes visible to later compute passes.
        void BarrierSchedulingBuffers(const Ref<CommandBuffer>& cmd) const;

        // Clears persistent particle state before a released allocation is reused.
        void ClearParticleAllocation(const SSystemInstanceAllocation& allocation);

        SResourcePoolLimits m_Limits;
        ParticleStateLayoutRegistry m_ParticleStateLayouts;
        std::vector<SParticleStateLayoutRuntime> m_ParticleStateLayoutRuntimes;
        std::unordered_set<SSystemInstanceKey> m_AllocationFailures;
        std::unordered_set<SSystemInstanceKey> m_UnsupportedParticleStateLayoutInstances;
        ResourcePool m_ResourcePool;
        std::unordered_map<SSystemInstanceKey, SInstanceRecord> m_InstanceRecords;
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

        Ref<Shader> m_SchedulerBeginShader;
        Ref<ComputePipeline> m_SchedulerBeginPipeline;
        Ref<Shader> m_SchedulerInitEmittersShader;
        Ref<ComputePipeline> m_SchedulerInitEmittersPipeline;
        Ref<Shader> m_SchedulerScheduleEmittersShader;
        Ref<ComputePipeline> m_SchedulerScheduleEmittersPipeline;
        Ref<Shader> m_SchedulerFinalizeShader;
        Ref<ComputePipeline> m_SchedulerFinalizePipeline;

        float m_LastDeltaTimeSeconds = 0.0f;
        float m_ElapsedTimeSeconds = 0.0f;

        uint64_t m_SubmissionSerial = 0;
        SSimulationMetrics m_LastMetrics{};

        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}