#include "ParticleData.hlsl"
#include "StrandMath.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<SGPUParticle> particles;
// Position, previous position, and transported normal occupy three float4 slots per pointIndex.
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> strandPoints;
[[vk::binding(0, 1)]] cbuffer cbParams { float4 TimeData; float4 ViewportData; };

struct PushConstants
{
    float4x4 RootTransform;
    uint ParticleOffset;
    uint PointOffset;
    uint PointCount;
    uint StrandCount;
    float SegmentLength;
    float Stiffness;
    float Damping;
    float Sway;
    float3 Gravity;
    uint Generation;
    uint SolverIterations;
};
[[vk::push_constant]] PushConstants pc;

float3 LoadPoint(uint base, uint index) { return strandPoints[(base + index) * 3].xyz; }

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.StrandCount) return;
    SGPUParticle root = particles[pc.ParticleOffset + id.x];
    uint count = pc.PointCount;
    if (count < 2 || count > 96) return;
    uint base = pc.PointOffset + id.x * count;
    if (root.Alive < 0.5)
    {
        // Generation zero marks an inactive chain even if its slot respawns with the same order.
        strandPoints[base * 3 + 1].w = 0.0;
        return;
    }
    float3 anchor = mul(pc.RootTransform, float4(root.Position, 1)).xyz;
    float3x3 linearTransform = (float3x3)pc.RootTransform;
    float transformScale = max(length(mul(linearTransform, float3(1, 0, 0))),
        max(length(mul(linearTransform, float3(0, 1, 0))), length(mul(linearTransform, float3(0, 0, 1)))));
    float lengthPerSegment = pc.SegmentLength * max(transformScale, 0.0001);
    float seed = float(id.x) * 0.173 + float(root.LinkOrder % 65536) * 0.013;
    bool initialize = asuint(strandPoints[base * 3].w) != root.LinkOrder ||
        asuint(strandPoints[base * 3 + 1].w) != pc.Generation;
    float3 predicted[96];
    float3 previous[96];
    if (initialize)
    {
        float3 direction = safeNormalize3(float3(hash31(float3(seed, 2, 3)) - 0.5,
            hash31(float3(seed, 4, 5)) - 0.5, hash31(float3(seed, 6, 7)) - 0.5), float3(0, 1, 0));
        float3 axis = safeNormalize3(cross(direction, float3(0, 0, 1)), float3(1, 0, 0));
        predicted[0] = anchor;
        previous[0] = anchor;
        for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
        {
            predicted[pointIndex] = predicted[pointIndex - 1] + direction * lengthPerSegment;
            previous[pointIndex] = predicted[pointIndex];
            direction = rotateAroundAxis(direction, axis, 0.4);
        }
    }
    else
    {
        for (uint pointIndex = 0; pointIndex < count; ++pointIndex)
        {
            predicted[pointIndex] = LoadPoint(base, pointIndex);
            previous[pointIndex] = strandPoints[(base + pointIndex) * 3 + 1].xyz;
        }
        // Bounded internal steps keep the constraints stable at low frame rates.
        float duration = min(max(TimeData.x, 0.0), 8.0 / 60.0);
        uint steps = max(1u, uint(ceil(duration * 60.0)));
        float dt = duration / float(steps);
        float damping = pow(pc.Damping, dt * 60.0);
        float3 oldAnchor = predicted[0];
        float3 gravity = mul((float3x3)pc.RootTransform, pc.Gravity);
        float3 rootVelocity = duration > 0.000001 ? (anchor - oldAnchor) / duration : float3(0, 0, 0);
        float3 drag = -rootVelocity * 0.8;
        for (uint step = 0; step < steps && dt > 0; ++step)
        {
            predicted[0] = lerp(oldAnchor, anchor, float(step + 1) / float(steps));
            for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
            {
                float3 position = predicted[pointIndex];
                float3 noise = curlNoise3(position * 2.2 + float3(seed * 37, seed * 21, seed * 11) +
                    (TimeData.y - duration + float(step + 1) * dt) * float3(0.12, 0.09, 0.07));
                predicted[pointIndex] += (position - previous[pointIndex]) * damping +
                    (gravity + drag + noise * pc.Sway) * dt * dt;
                previous[pointIndex] = position;
            }
            for (uint iteration = 0; iteration < pc.SolverIterations; ++iteration)
            {
                for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
                {
                    float3 delta = predicted[pointIndex] - predicted[pointIndex - 1];
                    float distance = length(delta);
                    if (distance > 0.000001)
                    {
                        float3 correction = delta / distance * (distance - lengthPerSegment) * pc.Stiffness;
                        if (pointIndex == 1) predicted[pointIndex] -= correction;
                        else
                        {
                            predicted[pointIndex - 1] += correction * 0.5;
                            predicted[pointIndex] -= correction * 0.5;
                        }
                    }
                }
            }
        }
        // The root remains pinned even when the timeline is paused.
        predicted[0] = anchor;
    }
    float3 tangent = safeNormalize3(predicted[1] - predicted[0], float3(0, 1, 0));
    float3 reference = abs(tangent.z) < 0.9 ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 normal = safeNormalize3(cross(tangent, reference), float3(1, 0, 0));
    for (uint pointIndex = 0; pointIndex < count; ++pointIndex)
    {
        float3 nextTangent = safeNormalize3(predicted[min(pointIndex + 1, count - 1)] -
            predicted[pointIndex > 0 ? pointIndex - 1 : 0], tangent);
        float3 axis = cross(tangent, nextTangent);
        float axisLength = length(axis);
        if (axisLength > 0.000001)
            normal = rotateAroundAxis(normal, axis / axisLength,
                atan2(axisLength, clamp(dot(tangent, nextTangent), -1.0, 1.0)));
        normal = safeNormalize3(normal - nextTangent * dot(normal, nextTangent), normal);
        strandPoints[(base + pointIndex) * 3] = float4(predicted[pointIndex], asfloat(root.LinkOrder));
        strandPoints[(base + pointIndex) * 3 + 1] = float4(previous[pointIndex], asfloat(pc.Generation));
        strandPoints[(base + pointIndex) * 3 + 2] = float4(normal, 0);
        tangent = nextTangent;
    }
}
