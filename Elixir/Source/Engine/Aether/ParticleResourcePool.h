#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    struct SBufferRange
    {
        uint32_t Offset = 0;
        uint32_t Count = 0;

        explicit operator bool() const { return Count != 0; }
    };

    struct SParticlePoolLimits
    {
        uint32_t MaxSystemInstances = 256;
        uint32_t ParticleCapacity = 1'000'000;
        uint32_t EmitterCapacity = 4'096;
        uint32_t OpCapacity = 65'536;
        uint32_t ParameterCapacity = 16'384;
        uint32_t TriggerTargetCapacity = 4'096;
        uint32_t TriggerEventCapacityPerEmitter = 64;
    };

    struct SSystemInstanceAllocation
    {
        uint32_t InstanceIndex = UINT32_MAX;
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

    class ParticleResourcePool final
    {
    public:
        explicit ParticleResourcePool(const SParticlePoolLimits& limits);

        std::optional<SSystemInstanceAllocation> Allocate(const SCompiledSystem& system);
        void Release(const SSystemInstanceAllocation& allocation);

        const SParticlePoolLimits& GetLimits() const { return m_Limits; }

    private:
        static SBufferRange AllocateRange(std::vector<SBufferRange>& freeRanges, uint32_t count);
        static void ReleaseRange(std::vector<SBufferRange>& freeRanges, SBufferRange range);

        static std::vector<SBufferRange> MakeFreeRanges(uint32_t capacity);

        SParticlePoolLimits m_Limits;

        std::vector<SBufferRange> m_FreeInstanceSlots;
        std::vector<uint32_t> m_InstanceGenerations;

        std::vector<SBufferRange> m_FreeParticles;
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
