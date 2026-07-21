#include "epch.h"
#include "ParticleResourcePool.h"

namespace Elixir::Aether
{
    ParticleResourcePool::ParticleResourcePool(const SParticlePoolLimits& limits)
      : m_Limits(limits),
        m_FreeInstanceSlots(MakeFreeRanges(limits.MaxSystemInstances)),
        m_InstanceGenerations(limits.MaxSystemInstances, 0),
        m_FreeParticles(MakeFreeRanges(limits.ParticleCapacity)),
        m_FreeEmitters(MakeFreeRanges(limits.EmitterCapacity)),
        m_FreeOps(MakeFreeRanges(limits.OpCapacity)),
        m_FreeParameters(MakeFreeRanges(limits.ParameterCapacity)),
        m_FreeTriggerTargets(MakeFreeRanges(limits.TriggerTargetCapacity)),
        m_FreeEmitterStates(MakeFreeRanges(limits.EmitterCapacity)),
        m_FreeSpawnRequests(MakeFreeRanges(limits.EmitterCapacity)),
        m_FreeTriggerEvents(MakeFreeRanges(
            limits.EmitterCapacity * limits.TriggerEventCapacityPerEmitter
        )),
        m_FreeTriggerQueueStates(MakeFreeRanges(limits.EmitterCapacity * 2)) {}

    std::optional<SSystemInstanceAllocation> ParticleResourcePool::Allocate(
        const SCompiledSystem& system
    )
    {
        const auto particleCount = system.TotalMaxParticles;
        const auto emitterCount = (uint32_t)system.Emitters.size();
        const auto opCount = (uint32_t)system.Ops.size();
        const auto parameterCount = (uint32_t)system.Parameters.size();
        const auto triggerTargetCount = (uint32_t)system.TriggerTargets.size();
        const auto emitterStateCount = emitterCount;
        const auto spawnRequestCount = emitterCount;
        const auto triggerEventCount = emitterCount * m_Limits.TriggerEventCapacityPerEmitter;
        const auto triggerQueueStateCount = emitterCount * 2;

        SSystemInstanceAllocation allocation{};

        const auto instanceSlot = AllocateRange(m_FreeInstanceSlots, 1);

        allocation.Particles = AllocateRange(m_FreeParticles, particleCount);
        allocation.Emitters = AllocateRange(m_FreeEmitters, emitterCount);
        allocation.Ops = AllocateRange(m_FreeOps, opCount);
        allocation.Parameters = AllocateRange(m_FreeParameters, parameterCount);
        allocation.TriggerTargets = AllocateRange(m_FreeTriggerTargets, triggerTargetCount);
        allocation.EmitterStates = AllocateRange(m_FreeEmitterStates, emitterStateCount);
        allocation.SpawnRequests = AllocateRange(m_FreeSpawnRequests, spawnRequestCount);
        allocation.TriggerEvents = AllocateRange(m_FreeTriggerEvents, triggerEventCount);
        allocation.TriggerQueueStates =
            AllocateRange(m_FreeTriggerQueueStates, triggerQueueStateCount);

        const bool allocationFailed =
            instanceSlot.Count == 0 ||
            allocation.Particles.Count != particleCount ||
            allocation.Emitters.Count != emitterCount ||
            allocation.Ops.Count != opCount ||
            allocation.Parameters.Count != parameterCount ||
            allocation.TriggerTargets.Count != triggerTargetCount ||
            allocation.EmitterStates.Count != emitterStateCount ||
            allocation.SpawnRequests.Count != spawnRequestCount ||
            allocation.TriggerEvents.Count != triggerEventCount ||
            allocation.TriggerQueueStates.Count != triggerQueueStateCount;

        if (allocationFailed)
        {
            Release(allocation);
            ReleaseRange(m_FreeInstanceSlots, instanceSlot);
            return std::nullopt;
        }

        allocation.InstanceIndex = instanceSlot.Offset;

        auto& generation = m_InstanceGenerations[allocation.InstanceIndex];
        ++generation;

        // Generation zero is reserved for never-initialized GPU scheduler state.
        if (generation == 0)
            ++generation;

        allocation.Generation = generation;
        return allocation;
    }

    void ParticleResourcePool::Release(const SSystemInstanceAllocation& allocation)
    {
        ReleaseRange(m_FreeParticles, allocation.Particles);
        ReleaseRange(m_FreeEmitters, allocation.Emitters);
        ReleaseRange(m_FreeOps, allocation.Ops);
        ReleaseRange(m_FreeParameters, allocation.Parameters);
        ReleaseRange(m_FreeTriggerTargets, allocation.TriggerTargets);
        ReleaseRange(m_FreeEmitterStates, allocation.EmitterStates);
        ReleaseRange(m_FreeSpawnRequests, allocation.SpawnRequests);
        ReleaseRange(m_FreeTriggerEvents, allocation.TriggerEvents);
        ReleaseRange(m_FreeTriggerQueueStates, allocation.TriggerQueueStates);

        if (allocation.InstanceIndex != UINT32_MAX)
            ReleaseRange(m_FreeInstanceSlots, { allocation.InstanceIndex, 1 });
    }

    SBufferRange ParticleResourcePool::AllocateRange(
        std::vector<SBufferRange>& freeRanges,
        const uint32_t count
    )
    {
        if (count == 0)
            return {};

        for (auto it = freeRanges.begin(); it != freeRanges.end(); ++it)
        {
            if (it->Count < count)
                continue;

            const SBufferRange result{ it->Offset, count };
            it->Offset += count;
            it->Count -= count;

            if (it->Count == 0)
                freeRanges.erase(it);

            return result;
        }

        return {};
    }

    void ParticleResourcePool::ReleaseRange(
        std::vector<SBufferRange>& freeRanges,
        const SBufferRange range
    )
    {
        if (range.Count == 0)
            return;

        freeRanges.push_back(range);
        std::ranges::sort(freeRanges, {}, &SBufferRange::Offset);

        std::vector<SBufferRange> merged;
        merged.reserve(freeRanges.size());

        for (const auto& current : freeRanges)
        {
            if (!merged.empty() &&
                merged.back().Offset + merged.back().Count == current.Offset)
            {
                merged.back().Count += current.Count;
                continue;
            }

            merged.push_back(current);
        }

        freeRanges = std::move(merged);
    }

    std::vector<SBufferRange> ParticleResourcePool::MakeFreeRanges(const uint32_t capacity)
    {
        if (capacity == 0)
            return {};

        return {{ 0, capacity }};
    }
}
