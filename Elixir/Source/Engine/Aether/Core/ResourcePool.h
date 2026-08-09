#pragma once

#include <Engine/Aether/System.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>

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

    /**
     * @brief Allocates shared logical resource ranges for Aether system instances.
     *
     * The pool reserves contiguous ranges for all tables required by a compiled
     * system. Allocation is atomic from the caller's perspective: when any required
     * range is unavailable, all ranges reserved by that request are released.
     *
     * The pool owns allocation policy only. The particle renderer owns the GPU buffers
     * that correspond to these ranges and retires their contents after GPU
     * synchronization.
     *
     * @thread_safety Not synchronized. Access it only from the renderer's
     * allocation and retirement path.
     */
    class ELIXIR_API ResourcePool final
    {
    public:
        /**
         * @brief Creates a pool with fixed shared-resource capacities.
         *
         * @param limits Maximum capacity of each logical resource table.
         * @param layouts Registered particle-state layouts supported by the renderer.
         *
         * @pre Each registered layout has a non-zero particle capacity.
         */
        explicit ResourcePool(
            const SResourcePoolLimits& limits,
            const ParticleStateLayoutRegistry& layouts
        );

        /**
         * @brief Reserves all ranges required by a compiled system.
         *
         * @param system Compiled system whose tables require allocation.
         * @return A complete allocation when all required ranges are available.
         * @return std::nullopt when any required capacity is unavailable or the
         * system requests an unsupported particle-state layout.
         */
        std::optional<SSystemInstanceAllocation> Allocate(const SCompiledSystem& system);

        /**
         * @brief Releases all ranges owned by an allocation.
         *
         * @param allocation Allocation previously returned by Allocate().
         *
         * @pre allocation has not already been released.
         */
        void Release(const SSystemInstanceAllocation& allocation);

        /**
         * @brief Returns the fixed capacity limits of this pool.
         * @return Resource limits selected during construction.
         */
        const SResourcePoolLimits& GetLimits() const { return m_Limits; }

    private:
        // Reserves one contiguous range from a free-list.
        static SBufferRange AllocateRange(std::vector<SBufferRange>& freeRanges, uint32_t count);

        // Returns a range to a free-list and merges adjacent ranges.
        static void ReleaseRange(std::vector<SBufferRange>& freeRanges, SBufferRange range);

        // Creates one free-range spanning an entire table.
        static std::vector<SBufferRange> MakeFreeRanges(uint32_t capacity);

        // Finds the free-list associated with a particle-state layout.
        std::vector<SBufferRange>* FindParticleFreeRanges(EParticleStateLayout layout);

        SResourcePoolLimits m_Limits;

        // Tracks free particle-state ranges for one registered layout.
        struct SParticleStateLayoutAllocator
        {
            EParticleStateLayout Key = EParticleStateLayout::CoreV1;
            std::vector<SBufferRange> FreeParticleRanges;
        };
        std::vector<SParticleStateLayoutAllocator> m_ParticleStateLayoutAllocators;

        std::vector<SBufferRange> m_FreeInstanceSlots;
        std::vector<uint32_t> m_InstanceGenerations;

        std::vector<SBufferRange> m_FreeEmitters;
        std::vector<SBufferRange> m_FreeOps;
        std::vector<SBufferRange> m_FreeParameters;
        std::vector<SBufferRange> m_FreeTriggerTargets;
        std::vector<SBufferRange> m_FreeEmitterStates;
        std::vector<SBufferRange> m_FreeSpawnRequests;
        std::vector<SBufferRange> m_FreeTriggerEvents;
        std::vector<SBufferRange> m_FreeTriggerQueueStates;
    };
}
