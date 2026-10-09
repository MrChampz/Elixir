#include "epch.h"
#include "ResourcePool.h"

namespace Elixir::Aether::Core
{
    ResourcePool::ResourcePool(
        const SResourcePoolLimits& limits,
        const ParticleStateLayoutRegistry& layouts
    ) : m_Limits(limits),
        m_FreeInstanceSlots(MakeFreeRanges(limits.MaxSystemInstances)),
        m_InstanceGenerations(limits.MaxSystemInstances, 0),
        m_FreeEmitters(MakeFreeRanges(limits.EmitterCapacity)),
        m_FreeOperations(MakeFreeRanges(limits.OperationCapacity)),
        m_FreeOperands(MakeFreeRanges(limits.OperandCapacity)),
        m_FreeConstants(MakeFreeRanges(limits.ConstantCapacity)),
        m_FreeParameters(MakeFreeRanges(limits.ParameterWordCapacity)),
        m_FreeTriggerTargets(MakeFreeRanges(limits.TriggerTargetCapacity)),
        m_FreeEmitterStates(MakeFreeRanges(limits.EmitterCapacity)),
        m_FreeSpawnRequests(MakeFreeRanges(limits.EmitterCapacity)),
        m_FreeTriggerEvents(MakeFreeRanges(
            limits.EmitterCapacity * limits.TriggerEventCapacityPerEmitter
        )),
        m_FreeTriggerQueueStates(MakeFreeRanges(limits.EmitterCapacity * 2))
    {
        for (const auto& descriptor : layouts.GetDescriptors())
        {
            m_ParticleStateLayoutAllocators.push_back({
                .Key = descriptor.Key,
                .FreeParticleRanges = MakeFreeRanges(descriptor.ParticleCapacity),
            });
        }
    }

    std::optional<SSystemInstanceAllocation> ResourcePool::Allocate(
        const SCompiledSystem& system
    )
    {
        const auto particleCount = system.TotalMaxParticles;
        const auto emitterCount = (uint32_t)system.Emitters.size();
        const auto operationCount = (uint32_t)system.ModuleData.Operations.size();
        const auto operandCount = (uint32_t)system.ModuleData.Operands.size();
        const auto constantCount = (uint32_t)system.ModuleData.Constants.size();
        const auto parameterCount = (uint32_t)system.ParameterValues.size();
        const auto triggerTargetCount = (uint32_t)system.TriggerTargets.size();
        const auto emitterStateCount = emitterCount;
        const auto spawnRequestCount = emitterCount;
        const auto triggerEventCount = emitterCount * m_Limits.TriggerEventCapacityPerEmitter;
        const auto triggerQueueStateCount = emitterCount * 2;

        SSystemInstanceAllocation allocation{};

        auto* particleFreeRanges = FindParticleFreeRanges(system.ParticleStateLayout);
        if (!particleFreeRanges)
            return std::nullopt;

        allocation.ParticleStateLayout = system.ParticleStateLayout;

        const auto instanceSlot = AllocateRange(m_FreeInstanceSlots, 1);
        allocation.Particles = AllocateRange(*particleFreeRanges, particleCount);
        allocation.Emitters = AllocateRange(m_FreeEmitters, emitterCount);
        const auto sharedProgram = std::ranges::find_if(m_Programs, [&system](const auto& program)
        {
            return program.SourceId == system.SourceId && program.CompilationRevision == system.CompilationRevision;
        });
        if (sharedProgram != m_Programs.end())
        {
            ++sharedProgram->ReferenceCount;
            allocation.ProgramAllocationId = sharedProgram->Id;
            allocation.Operations = sharedProgram->Operations;
            allocation.Operands = sharedProgram->Operands;
            allocation.Constants = sharedProgram->Constants;
        }
        else
        {
            allocation.Operations = AllocateRange(m_FreeOperations, operationCount);
            allocation.Operands = AllocateRange(m_FreeOperands, operandCount);
            allocation.Constants = AllocateRange(m_FreeConstants, constantCount);
            allocation.UploadProgram = true;
        }
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
            allocation.Operations.Count != operationCount ||
            allocation.Operands.Count != operandCount ||
            allocation.Constants.Count != constantCount ||
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

        if (allocation.UploadProgram)
        {
            allocation.ProgramAllocationId = m_NextProgramAllocationId++;
            m_Programs.push_back({
                allocation.ProgramAllocationId, system.SourceId, system.CompilationRevision, 1,
                allocation.Operations, allocation.Operands, allocation.Constants
            });
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

    void ResourcePool::Release(const SSystemInstanceAllocation& allocation)
    {
        auto* particleFreeRanges = FindParticleFreeRanges(allocation.ParticleStateLayout);
        EE_CORE_ASSERT(
            particleFreeRanges,
            "Aether attempted to release an unknown particle state layout."
        )
        if (!particleFreeRanges) return;

        ReleaseRange(*particleFreeRanges, allocation.Particles);
        ReleaseRange(m_FreeEmitters, allocation.Emitters);
        ReleaseProgram(allocation);
        ReleaseRange(m_FreeParameters, allocation.Parameters);
        ReleaseRange(m_FreeTriggerTargets, allocation.TriggerTargets);
        ReleaseRange(m_FreeEmitterStates, allocation.EmitterStates);
        ReleaseRange(m_FreeSpawnRequests, allocation.SpawnRequests);
        ReleaseRange(m_FreeTriggerEvents, allocation.TriggerEvents);
        ReleaseRange(m_FreeTriggerQueueStates, allocation.TriggerQueueStates);

        if (allocation.InstanceIndex != UINT32_MAX)
            ReleaseRange(m_FreeInstanceSlots, { allocation.InstanceIndex, 1 });
    }

    void ResourcePool::ReleaseProgram(const SSystemInstanceAllocation& allocation)
    {
        if (allocation.ProgramAllocationId != 0)
        {
            const auto found = std::ranges::find(m_Programs, allocation.ProgramAllocationId, &SProgramAllocation::Id);
            EE_CORE_ASSERT(found != m_Programs.end(), "Aether attempted to release an unknown program.")
            if (found == m_Programs.end()) return;
            if (--found->ReferenceCount != 0) return;
            m_Programs.erase(found);
        }
        ReleaseRange(m_FreeOperations, allocation.Operations);
        ReleaseRange(m_FreeOperands, allocation.Operands);
        ReleaseRange(m_FreeConstants, allocation.Constants);
    }

    SBufferRange ResourcePool::AllocateRange(
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

    void ResourcePool::ReleaseRange(
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

    std::vector<SBufferRange> ResourcePool::MakeFreeRanges(const uint32_t capacity)
    {
        if (capacity == 0)
            return {};

        return {{ 0, capacity }};
    }

    std::vector<SBufferRange>* ResourcePool::FindParticleFreeRanges(
        const EParticleStateLayout layout
    )
    {
        for (auto& allocator : m_ParticleStateLayoutAllocators)
        {
            if (allocator.Key == layout)
                return &allocator.FreeParticleRanges;
        }

        return nullptr;
    }
}
