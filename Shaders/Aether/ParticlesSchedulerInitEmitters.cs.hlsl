#include "ParticlesSchedulerCommon.hlsl"

[[vk::binding(4, 0)]]
StructuredBuffer<SystemInstance> instances;

[[vk::binding(5, 0)]]
RWStructuredBuffer<EmitterInstanceState> emitterStates;

[[vk::binding(10, 0)]]
RWStructuredBuffer<TriggerQueueState> triggerQueueStates;

[[vk::binding(11, 0)]]
RWStructuredBuffer<SystemSchedulerState> schedulerStates;

struct PushConstants
{
    uint InstanceIndex;
};

[[vk::push_constant]]
PushConstants pc;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SystemInstance instance = instances[pc.InstanceIndex];

    uint localEmitterIndex = dispatchThreadId.x;
    if (localEmitterIndex >= instance.EmitterCount)
        return;

    SystemSchedulerState scheduler = schedulerStates[pc.InstanceIndex];

    if (scheduler.ResetPending != 0)
    {
        EmitterInstanceState state = (EmitterInstanceState)0;
        state.Generation = instance.Generation;
        emitterStates[EmitterStateIndex(instance, localEmitterIndex)] = state;

        triggerQueueStates[TriggerQueueStateIndex(instance, 0, localEmitterIndex)]
            = (TriggerQueueState)0;
        triggerQueueStates[TriggerQueueStateIndex(instance, 1, localEmitterIndex)]
            = (TriggerQueueState)0;

        return;
    }

    uint writeBufferIndex = 1 - scheduler.ActiveTriggerBufferIndex;
    triggerQueueStates[TriggerQueueStateIndex(instance, writeBufferIndex, localEmitterIndex)]
        = (TriggerQueueState)0;
}