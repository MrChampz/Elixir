#include "ParticlesSchedulerCommon.hlsl"

[[vk::binding(4, 0)]]
StructuredBuffer<SGPUSystemInstance> instances;

[[vk::binding(11, 0)]]
RWStructuredBuffer<SGPUSystemSchedulerState> schedulerStates;

struct PushConstants
{
    uint InstanceIndex;
};

[[vk::push_constant]]
PushConstants pc;

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SGPUSystemInstance instance = instances[pc.InstanceIndex];
    SGPUSystemSchedulerState scheduler = schedulerStates[pc.InstanceIndex];
    const bool mustReset = scheduler.Generation != instance.Generation;

    if (mustReset)
    {
        scheduler.Generation = instance.Generation;
        scheduler.ActiveTriggerBufferIndex = 0;
    }

    scheduler.ResetPending = mustReset ? 1 : 0;
    schedulerStates[pc.InstanceIndex] = scheduler;
}