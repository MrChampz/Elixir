#include "../../Shaders/Aether/ParticleData.hlsl"
#include "../../Shaders/Aether/Math.hlsl"
#include "VegetationSurfaces.hlsl"

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

// One invocation owns a main chain and all its branches. Parent writes complete before
// branch anchors are read, so no cross-invocation synchronization is needed.
void SolveChain(uint rootSlot, uint branch, uint mainSlot)
{
    uint slots[96];
    float3 positions[96];
    float3 previous[96];
    float restLengths[96];
    uint count = 0;
    uint slot = rootSlot;
    SGPUParticle root = particles[pc.ParticleOffset + rootSlot];
    bool development = ChainTopology.x != 0;
    float capacityLength = float(pc.MaxPoints - 1) * pc.SegmentLength;
    float maxLength = ChainGrowth.y > 0 ? min(ChainGrowth.y, capacityLength) : capacityLength;
    float developmentAge = max(root.Transform.z, 0);
    if (development && ChainTopology.w == 0) developmentAge += pc.DeltaSeconds;
    float age = developmentAge;
    float branchScale = branch > 0 ? ChainGrowth.z : 1;
    uint attachPoint = 0;
    float3 anchor = root.Position - root.Velocity * pc.DeltaSeconds;
    if (branch > 0)
    {
        uint seed = ChainCounts.x ^ particles[pc.ParticleOffset + mainSlot].GroupId;
        float jitter = (ChainRandom(seed + branch * 17) * 2 - 1) *
            float(ChainTopology.z) * 0.5 * ChainGrowth.w;
        attachPoint = uint(clamp(float(branch * ChainTopology.z) + jitter,
            1.0, float(pc.MaxPoints - 2)));
        uint parent = (mainSlot + attachPoint * pc.OrderStep) % pc.ParticleCount;
        SGPUParticle parentPoint = particles[pc.ParticleOffset + parent];
        anchor = parentPoint.Position;
        developmentAge = particles[pc.ParticleOffset + mainSlot].Transform.z;
        age = developmentAge;
        age -= float(attachPoint) * pc.SegmentLength / max(ChainGrowth.x, 0.000001) +
            ChainRandom(seed + branch * 73) * ChainClimbDelay.w;
        if (parentPoint.Alive < 0.5) age = -1;
    }
    maxLength *= branchScale;
    float grown = maxLength;
    if (development && ChainTopology.w != 1 && ChainGrowth.x > 0)
        grown = clamp(age * ChainGrowth.x * branchScale, 0, maxLength);
    float grownSegments = grown / pc.SegmentLength;

    for (uint pointIndex = 0; pointIndex < pc.MaxPoints; ++pointIndex)
    {
        uint index = pc.ParticleOffset + slot;
        slots[count] = index;
        float3 position = particles[index].Position;
        previous[count] = position - particles[index].Velocity * pc.DeltaSeconds;
        positions[count] = position;
        restLengths[count] = development
            ? pc.SegmentLength * saturate(grownSegments - float(pointIndex) + 1)
            : pc.SegmentLength;
        ++count;
        uint next = NextSlot(slot);
        if (!Connected(slot, next)) break;
        slot = next;
    }
    if (pc.PinFirstParticle != 0 || branch > 0) positions[0] = anchor;
    bool pinEnd = ChainCounts.z != 0 && branch == 0 && count == pc.MaxPoints;
    float3 endPosition = ChainCounts.w == 1
        ? mul(ChainSimulation, float4(ChainEndPosition.xyz, 1)).xyz : ChainEndPosition.xyz;
    if (pinEnd) positions[count - 1] = endPosition;

    for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
    {
        float distance;
        float3 normal;
        if (ChainCounts.y > 0 && FindSurface(positions[pointIndex], distance, normal))
        {
            float3 climb = ChainClimbDelay.xyz - normal * dot(ChainClimbDelay.xyz, normal);
            float3 acceleration = mul((float3x3)ChainSimulation, climb) * ChainSurface.z;
            positions[pointIndex] += acceleration * pc.DeltaSeconds * pc.DeltaSeconds;
        }
        if (development && restLengths[pointIndex] == 0)
            positions[pointIndex] = positions[pointIndex - 1];
    }
    for (uint iteration = 0; iteration < pc.SolverIterations; ++iteration)
    {
        for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
        {
            float3 delta = positions[pointIndex] - positions[pointIndex - 1];
            float3 fallback = SafeNormalize(mul((float3x3)ChainSimulation,
                ChainClimbDelay.xyz), float3(0, 1, 0));
            if (branch > 0)
            {
                float angle = float(branch) * 2.39996323;
                fallback = SafeNormalize(fallback + float3(cos(angle), 0, sin(angle)), fallback);
            }
            float3 direction = SafeNormalize(delta, fallback);
            float3 correction = direction * (length(delta) - restLengths[pointIndex]) * pc.Stiffness;
            bool fixedPrevious = pointIndex == 1 && (pc.PinFirstParticle != 0 || branch > 0);
            bool fixedCurrent = pinEnd && pointIndex == count - 1;
            if (fixedPrevious && !fixedCurrent) positions[pointIndex] -= correction;
            else if (fixedCurrent && !fixedPrevious) positions[pointIndex - 1] += correction;
            else if (!fixedPrevious && !fixedCurrent)
            {
                positions[pointIndex - 1] += correction * 0.5;
                positions[pointIndex] -= correction * 0.5;
            }
        }
        for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
        {
            if (pinEnd && pointIndex == count - 1) continue;
            float stretch = length(positions[pointIndex] - positions[pointIndex - 1]);
            bool released = ChainSurface.w > 1 && stretch > pc.SegmentLength * ChainSurface.w;
            if (!released && (!development || float(pointIndex) <= ceil(grownSegments)))
                positions[pointIndex] = AttachSurface(positions[pointIndex]);
            if (development && restLengths[pointIndex] == 0)
                positions[pointIndex] = positions[pointIndex - 1];
        }
    }
    for (uint pointIndex = 0; pointIndex < count; ++pointIndex)
    {
        uint index = slots[pointIndex];
        particles[index].Position = positions[pointIndex];
        particles[index].Velocity = (positions[pointIndex] - previous[pointIndex]) / pc.DeltaSeconds;
        uint prevIdx = pointIndex > 0 ? pointIndex - 1 : 0;
        uint nextIdx = min(pointIndex + 1, count - 1);
        particles[index].Tangent = SafeNormalize(positions[nextIdx] - positions[prevIdx], float3(0, 1, 0));
        if (development)
        {
            // Reserved CoreV1 transform components expose development to visual attachments.
            particles[index].Transform.z = pointIndex == 0 ? developmentAge :
                saturate(grownSegments - float(pointIndex) + 1);
            particles[index].Transform.w = grownSegments - float(pointIndex);
        }
    }
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint rootSlot = id.x;
    if (rootSlot >= pc.ParticleCount || pc.MaxPoints < 2 || pc.MaxPoints > 96 ||
        pc.OrderStep == 0 || pc.DeltaSeconds <= 0.000001 ||
        particles[pc.ParticleOffset + rootSlot].Alive < 0.5) return;
    if (Connected(PreviousSlot(rootSlot), rootSlot)) return;
    uint branches = ChainTopology.x != 0 ? ChainTopology.y : 0;
    uint order = particles[pc.ParticleOffset + rootSlot].LinkOrder;
    if (branches > 0 && (order >= pc.OrderStep || order % (branches + 1) != 0)) return;
    SolveChain(rootSlot, 0, rootSlot);
    for (uint branch = 1; branch <= branches; ++branch)
    {
        uint branchSlot = (rootSlot + branch) % pc.ParticleCount;
        SGPUParticle candidate = particles[pc.ParticleOffset + branchSlot];
        if (candidate.Alive >= 0.5 && candidate.LinkOrder == order + branch)
            SolveChain(branchSlot, branch, rootSlot);
    }
}
