#pragma once

#include <Engine/Aether/System.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>
#include <Engine/Aether/Core/ResourceAllocation.h>

namespace Elixir::Aether::Core
{
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
