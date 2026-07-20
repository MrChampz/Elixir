#include "ParticlesSchedulerCommon.hlsl"

[[vk::binding(4, 0)]]
StructuredBuffer<SystemInstance> instances;

[[vk::binding(11, 0)]]
RWStructuredBuffer<SystemSchedulerState> schedulerStates;

struct PushConstants
{
    uint InstanceIndex;
};

[[vk::push_constant]]
PushConstants pc;

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SystemInstance instance = instances[pc.InstanceIndex];
    SystemSchedulerState scheduler = schedulerStates[pc.InstanceIndex];
    const bool mustReset = scheduler.Generation != instance.Generation;

    if (mustReset)
    {
        scheduler.Generation = instance.Generation;
        scheduler.ActiveTriggerBufferIndex = 0;
    }

    scheduler.ResetPending = mustReset ? 1 : 0;
    schedulerStates[pc.InstanceIndex] = scheduler;
}