#ifndef AETHER_PARTICLES_SCHEDULER_COMMON_HLSL
#define AETHER_PARTICLES_SCHEDULER_COMMON_HLSL

#include "ParticleData.hlsl"
#include "ParticleSpawnCommon.hlsl"

struct SGPUEmitterInstanceState
{
    float SpawnAccumulator;
    float BurstAccumulator;
    uint BufferCursor;
    uint EmissionIndex;
    uint Generation;
};

struct SGPUTriggerTarget
{
    uint TargetEmitterIndex;
    uint BurstCount;
    float DelaySeconds;
};

struct SGPUTriggerEvent
{
    float RemainingDelaySeconds;
    uint SpawnCount;
    uint Generation;
};

struct SGPUTriggerQueueState
{
    uint Count;
    uint OverflowCount;
};

struct SGPUSystemSchedulerState
{
    uint Generation;
    uint ActiveTriggerBufferIndex;
    uint ResetPending;
};

uint EmitterStateIndex(SGPUSystemInstance instance, uint localEmitterIndex)
{
    return instance.EmitterStateBaseOffset + localEmitterIndex;
}

uint SpawnRequestIndex(SGPUSystemInstance instance, uint localEmitterIndex)
{
    return instance.SpawnRequestBaseOffset + localEmitterIndex;
}

uint TriggerQueueStateIndex(
    SGPUSystemInstance instance,
    uint triggerBufferIndex,
    uint localEmitterIndex
)
{
    return instance.TriggerQueueStateBaseOffset +
        triggerBufferIndex * instance.EmitterCount +
        localEmitterIndex;
}

uint TriggerEventIndex(
    SGPUSystemInstance instance,
    uint localEmitterIndex,
    uint slot
)
{
    return instance.TriggerEventBaseOffset +
        localEmitterIndex * instance.TriggerEventCapacityPerEmitter +
        slot;
}

#endif
