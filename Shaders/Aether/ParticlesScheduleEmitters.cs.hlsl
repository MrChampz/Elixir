#include "ParticlesSchedulerCommon.hlsl"

[[vk::binding(1, 0)]]
StructuredBuffer<Emitter> emitters;

[[vk::binding(4, 0)]]
StructuredBuffer<SystemInstance> instances;

[[vk::binding(5, 0)]]
RWStructuredBuffer<EmitterInstanceState> emitterStates;

[[vk::binding(6, 0)]]
RWStructuredBuffer<SpawnRequest> spawnRequests;

[[vk::binding(7, 0)]]
StructuredBuffer<TriggerTarget> triggerTargets;

[[vk::binding(8, 0)]]
RWStructuredBuffer<TriggerEvent> triggerEventsA;

[[vk::binding(9, 0)]]
RWStructuredBuffer<TriggerEvent> triggerEventsB;

[[vk::binding(10, 0)]]
RWStructuredBuffer<TriggerQueueState> triggerQueueStates;

[[vk::binding(11, 0)]]
StructuredBuffer<SystemSchedulerState> schedulerStates;

cbuffer cbParams : register(b0)
{
    float4 Time;
    float4 Viewport;
};

struct PushConstants
{
    uint InstanceIndex;
};

[[vk::push_constant]]
PushConstants pc;

void AppendTriggerEvent(
    SystemInstance instance,
    uint writeBufferIndex,
    uint targetEmitterIndex,
    float delaySeconds,
    uint spawnCount
)
{
    uint queueIndex = TriggerQueueStateIndex(
        instance,
        writeBufferIndex,
        targetEmitterIndex
    );

    uint slot;
    InterlockedAdd(triggerQueueStates[queueIndex].Count, 1, slot);

    if (slot >= instance.TriggerEventCapacityPerEmitter)
    {
        uint ignored;
        InterlockedAdd(triggerQueueStates[queueIndex].OverflowCount, 1, ignored);
        return;
    }

    TriggerEvent event;
    event.RemainingDelaySeconds = delaySeconds;
    event.SpawnCount = spawnCount;
    event.Generation = instance.Generation;

    uint eventIndex = TriggerEventIndex(instance, targetEmitterIndex, slot);
    if (writeBufferIndex == 0)
        triggerEventsA[eventIndex] = event;
    else
        triggerEventsB[eventIndex] = event;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SystemInstance instance = instances[pc.InstanceIndex];

    uint localEmitterIndex = dispatchThreadId.x;
    if (localEmitterIndex >= instance.EmitterCount)
        return;

    uint emitterIndex = instance.EmitterBaseOffset + localEmitterIndex;
    Emitter emitter = emitters[emitterIndex];
    EmitterInstanceState state = emitterStates[EmitterStateIndex(instance, localEmitterIndex)];

    SystemSchedulerState scheduler = schedulerStates[pc.InstanceIndex];
    uint readBufferIndex = scheduler.ActiveTriggerBufferIndex;
    uint writeBufferIndex = 1 - readBufferIndex;

    uint maxParticles = (uint)emitter.MetaA.y;
    uint triggeredSpawnCount = 0;

    uint readQueueIndex = TriggerQueueStateIndex(instance, readBufferIndex, localEmitterIndex);
    uint eventCount = min(
        triggerQueueStates[readQueueIndex].Count,
        instance.TriggerEventCapacityPerEmitter
    );

    for (uint eventSlot = 0; eventSlot < eventCount; ++eventSlot)
    {
        uint eventIndex = TriggerEventIndex(instance, localEmitterIndex, eventSlot);

        TriggerEvent event;
        if (readBufferIndex == 0)
            event = triggerEventsA[eventIndex];
        else
            event = triggerEventsB[eventIndex];

        if (event.Generation != instance.Generation)
            continue;

        event.RemainingDelaySeconds -= Time.x;
        if (event.RemainingDelaySeconds <= 0.0f)
        {
            triggeredSpawnCount = min(
                maxParticles,
                triggeredSpawnCount + event.SpawnCount
            );
        }
        else
        {
            AppendTriggerEvent(
                instance,
                writeBufferIndex,
                localEmitterIndex,
                event.RemainingDelaySeconds,
                event.SpawnCount
            );
        }
    }

    state.SpawnAccumulator += max(emitter.MetaC.y, 0.0f) * Time.x;
    uint spawnCount = min((uint)state.SpawnAccumulator, maxParticles);
    state.SpawnAccumulator -= (float)spawnCount;

    bool isTriggerDriven = emitter.MetaD.y > 0.5f;
    uint burstCount = (uint)emitter.MetaD.x;
    float burstInterval = emitter.MetaC.w;

    if (!isTriggerDriven && burstCount > 0 && burstInterval > 0.0f)
    {
        state.BurstAccumulator = min(
            state.BurstAccumulator + Time.x,
            burstInterval * 8.0f
        );

        uint burstLoops = 0;
        while (state.BurstAccumulator >= burstInterval && burstLoops < 8)
        {
            state.BurstAccumulator -= burstInterval;
            spawnCount = min(maxParticles, spawnCount + burstCount);

            uint targetOffset = (uint)emitter.MetaB.z;
            uint targetCount = (uint)emitter.MetaB.w;

            for (uint targetIndex = 0; targetIndex < targetCount; ++targetIndex)
            {
                TriggerTarget target = triggerTargets[targetOffset + targetIndex];
                AppendTriggerEvent(
                    instance,
                    writeBufferIndex,
                    target.TargetEmitterIndex,
                    target.DelaySeconds,
                    target.BurstCount
                );
            }

            ++burstLoops;
        }
    }

    spawnCount = min(maxParticles, spawnCount + triggeredSpawnCount);

    SpawnRequest request;
    request.SpawnCursor = state.BufferCursor;
    request.SpawnCount = spawnCount;
    request.EmissionIndex = state.EmissionIndex;
    request.Generation = instance.Generation;
    spawnRequests[SpawnRequestIndex(instance, localEmitterIndex)] = request;

    state.EmissionIndex += spawnCount;
    state.BufferCursor = maxParticles > 0
        ? (state.BufferCursor + spawnCount) % maxParticles
        : 0;
    emitterStates[EmitterStateIndex(instance, localEmitterIndex)] = state;
}