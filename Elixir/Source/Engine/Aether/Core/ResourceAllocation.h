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
        uint32_t OperationCapacity = 65'536;
        /** Maximum number of four-byte operand references. */
        uint32_t OperandCapacity = 262'144;
        /** Maximum number of four-byte literal words. */
        uint32_t ConstantCapacity = 262'144;
        /** Maximum number of four-byte parameter words across live instances. */
        uint32_t ParameterWordCapacity = 65'536;
        uint32_t TriggerTargetCapacity = 4'096;
        uint32_t TriggerEventCapacityPerEmitter = 64;
        /** @brief Maximum persistent strand points shared by live instances; each point uses 48 bytes. */
        uint32_t StrandPointCapacity = 262'144;
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
        /** Identifies the shared program reservation owned by this allocation. */
        uint64_t ProgramAllocationId = 0;
        /** Upload the immutable program only when its reservation is first created. */
        bool UploadProgram = false;

        SBufferRange Particles;
        /** @brief Persistent strand point range owned by this instance. */
        SBufferRange StrandPoints;
        SBufferRange Emitters;
        SBufferRange Operations;
        SBufferRange Operands;
        SBufferRange Constants;
        /** Instance parameter range in four-byte words, relative to the parameter buffer. */
        SBufferRange Parameters;
        SBufferRange TriggerTargets;

        SBufferRange EmitterStates;
        SBufferRange SpawnRequests;
        SBufferRange TriggerEvents;
        SBufferRange TriggerQueueStates;
    };
}
