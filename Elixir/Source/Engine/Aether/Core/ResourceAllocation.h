#pragma once

namespace Elixir::Aether::Core
{
    /**
     * @brief Identifies a contiguous range in an Aether resource table.
     *
     * The range is expressed in elements, not bytes. Its offset is relative to the
     * beginning of the table that owns it.
     */
    struct SBufferRange
    {
        uint32_t Offset = 0;
        uint32_t Count = 0;

        explicit operator bool() const { return Count != 0; }
    };

    /**
     * @brief Defines the logical capacities available to Aether system instances.
     *
     * These limits bound the shared ranges allocated for compiled systems. They do
     * not create GPU buffers; the renderer creates GPU resources compatible with
     * these capacities.
     */
    struct SResourcePoolLimits
    {
        uint32_t MaxSystemInstances = 256;
        uint32_t ParticleCapacity = 1'000'000;
        uint32_t EmitterCapacity = 4'096;
        uint32_t OpCapacity = 65'536;
        uint32_t ParameterCapacity = 16'384;
        uint32_t TriggerTargetCapacity = 4'096;
        uint32_t TriggerEventCapacityPerEmitter = 64;
    };

    /**
     * @brief Stores the logical resource ranges assigned to one system instance.
     *
     * ResourcePool creates this allocation for a compiled system. The renderer
     * uses its ranges to upload system data and address the matching GPU tables.
     *
     * @note Ranges are valid only while the allocation remains owned by the pool.
     */
    struct SSystemInstanceAllocation
    {
        uint32_t InstanceIndex = UINT32_MAX;
        EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;
        uint32_t Generation = 1;

        SBufferRange Particles;
        SBufferRange Emitters;
        SBufferRange Ops;
        SBufferRange Parameters;
        SBufferRange TriggerTargets;

        SBufferRange EmitterStates;
        SBufferRange SpawnRequests;
        SBufferRange TriggerEvents;
        SBufferRange TriggerQueueStates;
    };
}
