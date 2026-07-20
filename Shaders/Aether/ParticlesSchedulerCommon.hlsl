struct SystemInstance
{
    uint ParticleBaseOffset;
    uint EmitterBaseOffset;
    uint OpBaseOffset;
    uint ParameterBaseOffset;

    uint EmitterStateBaseOffset;
    uint SpawnRequestBaseOffset;
    uint TriggerEventBaseOffset;
    uint TriggerQueueStateBaseOffset;

    uint ParticleCount;
    uint EmitterCount;
    uint TriggerEventCapacityPerEmitter;
    uint Generation;

    uint ParticleStateLayoutIndex;
};

struct Emitter
{
    float4 MetaA;
    float4 MetaB;
    float4 MetaC;
    float4 MetaD;
};

struct EmitterInstanceState
{
    float SpawnAccumulator;
    float BurstAccumulator;
    uint BufferCursor;
    uint EmissionIndex;
    uint Generation;
};

struct SpawnRequest
{
    uint SpawnCursor;
    uint SpawnCount;
    uint EmissionIndex;
    uint Generation;
};

struct TriggerTarget
{
    uint TargetEmitterIndex;
    uint BurstCount;
    float DelaySeconds;
};

struct TriggerEvent
{
    float RemainingDelaySeconds;
    uint SpawnCount;
    uint Generation;
};

struct TriggerQueueState
{
    uint Count;
    uint OverflowCount;
};

struct SystemSchedulerState
{
    uint Generation;
    uint ActiveTriggerBufferIndex;
    uint ResetPending;
};

uint EmitterStateIndex(SystemInstance instance, uint localEmitterIndex)
{
    return instance.EmitterStateBaseOffset + localEmitterIndex;
}

uint SpawnRequestIndex(SystemInstance instance, uint localEmitterIndex)
{
    return instance.SpawnRequestBaseOffset + localEmitterIndex;
}

uint TriggerQueueStateIndex(
    SystemInstance instance,
    uint triggerBufferIndex,
    uint localEmitterIndex
)
{
    return instance.TriggerQueueStateBaseOffset +
        triggerBufferIndex * instance.EmitterCount +
        localEmitterIndex;
}

uint TriggerEventIndex(
    SystemInstance instance,
    uint localEmitterIndex,
    uint slot
)
{
    return instance.TriggerEventBaseOffset +
        localEmitterIndex * instance.TriggerEventCapacityPerEmitter +
        slot;
}