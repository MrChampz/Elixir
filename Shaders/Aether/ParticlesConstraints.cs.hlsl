#include "ParticleData.hlsl"
#include "Math.hlsl"

[[vk::binding(0, 0)]]
RWStructuredBuffer<SGPUParticle> particles;

struct PushConstants
{
    uint ParticleOffset;
    uint ParticleCount;
    uint MaxPoints;
    uint OrderStep;
    float SegmentLength;
    float Stiffness;
    uint SolverIterations;
    uint PinFirstParticle;
    float DeltaSeconds;
};

[[vk::push_constant]]
PushConstants pc;

uint NextSlot(uint slot)
{
    uint step = pc.OrderStep % pc.ParticleCount;
    return slot >= pc.ParticleCount - step ? slot - (pc.ParticleCount - step) : slot + step;
}

uint PreviousSlot(uint slot)
{
    uint step = pc.OrderStep % pc.ParticleCount;
    return slot < step ? pc.ParticleCount - (step - slot) : slot - step;
}

bool Connected(uint firstSlot, uint secondSlot)
{
    uint a = pc.ParticleOffset + firstSlot;
    uint b = pc.ParticleOffset + secondSlot;
    uint order = particles[b].LinkOrder;

    return particles[a].Alive >= 0.5 && particles[b].Alive >= 0.5 &&
        particles[a].EmitterIndex == particles[b].EmitterIndex &&
        particles[a].GroupId == particles[b].GroupId && order >= pc.OrderStep &&
        order - pc.OrderStep == particles[a].LinkOrder &&
        (order / pc.OrderStep) % pc.MaxPoints != 0;
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint rootSlot = id.x;

    if (rootSlot >= pc.ParticleCount ||
        pc.MaxPoints < 2 ||
        pc.MaxPoints > 96 ||
        pc.OrderStep == 0 ||
        pc.DeltaSeconds <= 0.000001 ||
        particles[pc.ParticleOffset + rootSlot].Alive < 0.5)
        return;

    if (Connected(PreviousSlot(rootSlot), rootSlot))
        return;

    uint slots[96];
    float3 positions[96];
    float3 previous[96];

    uint count = 0;
    uint slot = rootSlot;
    for (uint pointIndex = 0; pointIndex < pc.MaxPoints; ++pointIndex)
    {
        uint index = pc.ParticleOffset + slot;

        float3 position = particles[index].Position;
        float3 velocity = particles[index].Velocity;

        slots[count] = index;
        previous[count] = position - velocity * pc.DeltaSeconds;
        positions[count] = position;
        
        ++count;

        uint next = NextSlot(slot);

        if (!Connected(slot, next))
            break;

        slot = next;
    }

    if (pc.PinFirstParticle != 0)
        positions[0] = previous[0];

    for (uint iteration = 0; iteration < pc.SolverIterations; ++iteration)
    {
        for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
        {
            float3 delta = positions[pointIndex] - positions[pointIndex - 1];
            float3 direction = SafeNormalize(delta, float3(0, 1, 0));
            float3 correction = direction * (length(delta) - pc.SegmentLength) * pc.Stiffness;

            if (pointIndex == 1 && pc.PinFirstParticle != 0)
            {
                positions[pointIndex] -= correction;   
            }
            else
            {
                positions[pointIndex - 1] += correction * 0.5;
                positions[pointIndex] -= correction * 0.5;
            }
        }
    }

    for (uint pointIndex = 0; pointIndex < count; ++pointIndex)
    {
        uint index = slots[pointIndex];

        particles[index].Position = positions[pointIndex];

        float3 delta = positions[pointIndex] - previous[pointIndex];
        particles[index].Velocity = delta / pc.DeltaSeconds;
        
        uint prevIdx = pointIndex > 0 ? pointIndex - 1 : 0;
        uint nextIdx = min(pointIndex + 1, count - 1);
        float3 tangent = positions[nextIdx] - positions[prevIdx];

        particles[index].Tangent = SafeNormalize(tangent, float3(0, 1, 0));
    }
}
