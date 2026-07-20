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

    if (scheduler.Generation != instance.Generation)
        return;

    scheduler.ActiveTriggerBufferIndex = 1 - scheduler.ActiveTriggerBufferIndex;
    schedulerStates[pc.InstanceIndex] = scheduler;
}