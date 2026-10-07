#include "ParticlesSchedulerCommon.hlsl"

[[vk::binding(4, 0)]]
StructuredBuffer<SGPUSystemInstance> instances;

[[vk::binding(5, 0)]]
RWStructuredBuffer<SGPUEmitterInstanceState> emitterStates;

[[vk::binding(10, 0)]]
RWStructuredBuffer<SGPUTriggerQueueState> triggerQueueStates;

[[vk::binding(11, 0)]]
RWStructuredBuffer<SGPUSystemSchedulerState> schedulerStates;

struct PushConstants
{
    uint InstanceIndex;
};

[[vk::push_constant]]
PushConstants pc;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SGPUSystemInstance instance = instances[pc.InstanceIndex];

    uint localEmitterIndex = dispatchThreadId.x;
    if (localEmitterIndex >= instance.EmitterCount)
        return;

    SGPUSystemSchedulerState scheduler = schedulerStates[pc.InstanceIndex];

    if (scheduler.ResetPending != 0)
    {
        SGPUEmitterInstanceState state = (SGPUEmitterInstanceState)0;
        state.Generation = instance.Generation;
        emitterStates[EmitterStateIndex(instance, localEmitterIndex)] = state;

        triggerQueueStates[TriggerQueueStateIndex(instance, 0, localEmitterIndex)]
            = (SGPUTriggerQueueState)0;
        triggerQueueStates[TriggerQueueStateIndex(instance, 1, localEmitterIndex)]
            = (SGPUTriggerQueueState)0;

        return;
    }

    uint writeBufferIndex = 1 - scheduler.ActiveTriggerBufferIndex;
    triggerQueueStates[TriggerQueueStateIndex(instance, writeBufferIndex, localEmitterIndex)]
        = (SGPUTriggerQueueState)0;
}