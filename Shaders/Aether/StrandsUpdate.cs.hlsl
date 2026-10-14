#include "ParticleData.hlsl"
#include "ParticleAttributes.hlsl"
#include "Operation.hlsl"
#include "StrandMath.hlsl"
#include "StrandSurfaces.hlsl"
#include "StrandGrowth.hlsl"
#include "StrandAnchors.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<SGPUParticle> particles;
// Each point stores position, previous position and transported normal.
// Normal.w stores grown segments at the root and the accumulated development front at point one.
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> strandPoints;
[[vk::binding(2, 0)]] StructuredBuffer<SGPUEmitter> emitters;
[[vk::binding(3, 0)]] StructuredBuffer<SGPUSystemInstance> instances;
[[vk::binding(4, 0)]] StructuredBuffer<SGPUOperation> operations;
[[vk::binding(5, 0)]] StructuredBuffer<uint> operands;
[[vk::binding(6, 0)]] ByteAddressBuffer constants;
[[vk::binding(7, 0)]] ByteAddressBuffer parameters;
#include "OperationExecutor.hlsl"

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
    uint Generation;
    uint SolverIterations;
    uint InstanceIndex;
    uint EmitterIndex;
    float GrowthSpeed;
    float GrowthMaxLength;
    uint BranchCount;
    uint BranchInterval;
    float BranchLengthScale;
};
[[vk::push_constant]] PushConstants pc;

// Converts world-space points into the module's simulation-space coordinates.
float3x3 InverseLinear(float3x3 value)
{
    float3 a = cross(value[1], value[2]);
    float determinant = dot(value[0], a);
    if (abs(determinant) < 0.000001) return float3x3(1,0,0, 0,1,0, 0,0,1);
    return transpose(float3x3(a, cross(value[2], value[0]), cross(value[0], value[1]))) / determinant;
}

float3 LoadPoint(uint base, uint index) { return strandPoints[(base + index) * 3].xyz; }

// Scores forward and sideways candidates against the already-grown path.
float3 GuideTip(SStrandSurfaces surfaces, float3 position, float3 normal, float3 forward,
    uint base, uint pointIndex, float segmentLength)
{
    float3 lateral = safeNormalize3(cross(normal, forward), float3(1, 0, 0));
    float3 bestDirection = forward;
    float bestScore = -1000;
    for (uint candidate = 0; candidate < 3u; ++candidate)
    {
        float side = candidate == 0u ? 0 : (candidate == 1u ? 0.9 : -0.9);
        float3 direction = safeNormalize3(forward + lateral * side, forward);
        float3 probe = position + direction * segmentLength * 2.0;
        uint surfaceIndex;
        float3 closest, nextNormal;
        if (FindSurface(surfaces, probe, surfaceIndex, closest, nextNormal))
            direction = SurfaceDirection(surfaces, nextNormal, direction);
        probe = position + direction * segmentLength * 2.0;
        float score = dot(direction, forward);
        for (uint older = 0; older + 3u < pointIndex; ++older)
        {
            float proximity = saturate(1.0 - length(probe - LoadPoint(base, older)) / max(segmentLength * 1.5, 0.000001));
            score -= proximity * 4.0;
        }
        if (score > bestScore) { bestScore = score; bestDirection = direction; }
    }
    return bestDirection;
}

void SimulateChain(SGPUParticle root, uint base, float3 anchor, float3 direction, float age, float chainScale, uint rootIndex, bool pinEnd, float3 endPosition)
{
    uint count = pc.PointCount;
    if (root.Alive < 0.5)
    {
        // Generation zero marks an inactive chain even if its slot respawns with the same order.
        strandPoints[base * 3 + 1].w = 0.0;
        return;
    }
    SGPUSystemInstance instance = instances[pc.InstanceIndex];
    SGPUEmitter emitter = emitters[pc.EmitterIndex];
    SOperationExecutor executor = CreateOperationExecutor(operands, constants, parameters);
    float3 origin = mul(pc.RootTransform, float4(0, 0, 0, 1)).xyz;

    float3x3 linearTransform = (float3x3)pc.RootTransform;
    float transformScale = max(length(mul(linearTransform, float3(1, 0, 0))),
        max(length(mul(linearTransform, float3(0, 1, 0))), length(mul(linearTransform, float3(0, 0, 1)))));
    float lengthPerSegment = pc.SegmentLength * max(transformScale, 0.0001) * chainScale;
    float maximum = float(count - 1) * pc.SegmentLength;
    if (pc.GrowthMaxLength > 0) maximum = min(maximum, pc.GrowthMaxLength);
    float grownLength = (age < 0 ? 0.0 : (pc.GrowthSpeed > 0 ? min(maximum, age * pc.GrowthSpeed) : maximum)) *
        max(transformScale, 0.0001) * chainScale;
    // Endpoint constraints require a fully grown chain; authoring rejects growing cables.
    if (pinEnd) grownLength = float(count - 1u) * lengthPerSegment;
    float grownSegments = grownLength / lengthPerSegment;
    uint activeCount = min(count, max(2u, uint(ceil(grownSegments)) + 1u));
    float seed = float(rootIndex) * 0.173 + float(root.LinkOrder % 65536) * 0.013;
    bool initialize = asuint(strandPoints[base * 3].w) != root.LinkOrder ||
        asuint(strandPoints[base * 3 + 1].w) != pc.Generation;
    SStrandSurfaces surfaces = strandSurfaces[pc.EmitterIndex];
    float3 predicted[96];
    float3 previous[96];
    uint attached[96];
    float cooldown[96];
    float3 tipGuidance = direction;
    if (initialize)
    {
        float3 axis = safeNormalize3(cross(direction, float3(0, 0, 1)), float3(1, 0, 0));
        predicted[0] = anchor;
        previous[0] = anchor;
        attached[0] = 0u;
        cooldown[0] = 0;
        for (uint pointIndex = 1; pointIndex < count; ++pointIndex)
        {
            predicted[pointIndex] = predicted[pointIndex - 1] + direction * clamp(grownLength - float(pointIndex - 1) * lengthPerSegment, 0.0, lengthPerSegment);
            previous[pointIndex] = predicted[pointIndex];
            attached[pointIndex] = 0u;
            cooldown[pointIndex] = 0;
            direction = rotateAroundAxis(direction, axis, 0.4);
            if (pinEnd)
            {
                float u = float(pointIndex) / float(count - 1u);
                float span = length(endPosition - anchor);
                float slack = sqrt(max(grownLength * grownLength - span * span, 0.0)) * .35;
                predicted[pointIndex] = lerp(anchor, endPosition, u) + float3(0, -4.0 * u * (1.0 - u) * slack, 0);
                previous[pointIndex] = predicted[pointIndex];
            }
        }
    }
    else
    {
        tipGuidance = safeNormalize3(strandPoints[base * 3u + 1u].xyz, direction);
        for (uint pointIndex = 0; pointIndex < count; ++pointIndex)
        {
            predicted[pointIndex] = LoadPoint(base, pointIndex);
            previous[pointIndex] = strandPoints[(base + pointIndex) * 3 + 1].xyz;
            uint marker = asuint(strandPoints[(base + pointIndex) * 3 + 1].w);
            attached[pointIndex] = pointIndex == 0u || marker > uint(surfaces.Controls.x) ? 0u : marker;
            cooldown[pointIndex] = pointIndex != 0u && asfloat(marker) < 0 ? -asfloat(marker) : 0;
        }
        // Bounded internal steps keep the constraints stable at low frame rates.
        float duration = min(max(TimeData.x, 0.0), 8.0 / 60.0);
        uint steps = max(1u, uint(ceil(duration * 60.0)));
        float dt = duration / float(steps);
        float damping = pow(pc.Damping, dt * 60.0);
        float3 oldAnchor = predicted[0];
        float3 oldEnd = predicted[count - 1u];
        float3x3 inverseLinear = InverseLinear(linearTransform);
        float3 rootVelocity = duration > 0.000001 ? (anchor - oldAnchor) / duration : float3(0, 0, 0);
        float3 drag = -rootVelocity * 0.8;
        for (uint step = 0; step < steps && dt > 0; ++step)
        {
            predicted[0] = lerp(oldAnchor, anchor, float(step + 1) / float(steps));
            if (pinEnd) predicted[count - 1u] = previous[count - 1u] = lerp(oldEnd, endPosition, float(step + 1) / float(steps));
            for (uint pointIndex = 1; pointIndex < activeCount; ++pointIndex)
            {
                cooldown[pointIndex] = max(0.0, cooldown[pointIndex] - dt);
                if (attached[pointIndex] == 0u || surfaces.Direction.w <= 1.0) continue;
                float stretch = length(predicted[pointIndex] - predicted[pointIndex - 1u]) / max(lengthPerSegment, 0.000001);
                if (pointIndex + 1u < activeCount)
                    stretch = max(stretch, length(predicted[pointIndex + 1u] - predicted[pointIndex]) / max(lengthPerSegment, 0.000001));
                // Measure the span to the previous remaining pin, so release propagates
                // through intervening free points even when constraint iterations hide local strain.
                uint previousPin = pointIndex - 1u;
                while (previousPin > 0u && attached[previousPin] == 0u) --previousPin;
                float spanLength = float(pointIndex - previousPin) * lengthPerSegment;
                stretch = max(stretch, length(predicted[pointIndex] - predicted[previousPin]) / max(spanLength, 0.000001));
                if (stretch > surfaces.Direction.w)
                {
                    attached[pointIndex] = 0u;
                    cooldown[pointIndex] = 0.5;
                    previous[pointIndex] = predicted[pointIndex];
                }
            }
            for (uint pointIndex = 1; pointIndex < activeCount; ++pointIndex)
            {
                if (pinEnd && pointIndex == count - 1u) continue;
                if (attached[pointIndex] != 0u) { predicted[pointIndex] = previous[pointIndex]; continue; }
                float3 position = predicted[pointIndex];
                SOperationExecutorContext context = (SOperationExecutorContext)0;
                context.Instance = instance;
                context.Attributes = LoadAttributes(root);
                SetAttribute(context.Attributes, ParticleAttribute_Position, float4(mul(inverseLinear, position - origin), 1));
                // Forces share the particle VM, accumulating a velocity increment from zero.
                SetAttribute(context.Attributes, ParticleAttribute_Velocity, float4(0, 0, 0, 0));
                context.DeltaTime = dt;
                context.Time = TimeData.y - duration + float(step + 1) * dt;
                context.NormalizedAge = saturate(root.Age / max(root.Lifetime, 0.001));
                context.DynamicInputs.DeltaTime = dt;
                context.DynamicInputs.Time = context.Time;
                context.DynamicInputs.NormalizedAge = context.NormalizedAge;
                context.DynamicInputs.ParticleSeed = seed;
                context.DynamicInputs.Random = CalculateDynamicInputRandom(seed, context.Time);
                for (uint operationIndex = 0; operationIndex < emitter.StrandUpdateOperationCount; ++operationIndex)
                {
                    context.OperationIndex = operationIndex;
                    executor.Execute(operations[emitter.StrandUpdateOperationOffset + operationIndex], context);
                }
                float3 deltaVelocity = GetAttribute(context.Attributes, ParticleAttribute_Velocity).xyz;
                predicted[pointIndex] += (position - previous[pointIndex]) * damping +
                    mul(linearTransform, deltaVelocity) * dt + drag * dt * dt;
                uint surfaceIndex;
                float3 closest, surfaceNormal;
                if (surfaces.Controls.x > 0 && FindSurface(surfaces, position, surfaceIndex, closest, surfaceNormal))
                {
                    // Use the previous completed segment, not an earlier point already advanced in this substep.
                    float3 heading = pointIndex > 1u ? LoadPoint(base, pointIndex - 1u) - LoadPoint(base, pointIndex - 2u) : direction;
                    float3 climb = SurfaceDirection(surfaces, surfaceNormal, heading);
                    if (pointIndex + 2u >= activeCount)
                        climb = GuideTip(surfaces, position, surfaceNormal, climb, base, pointIndex, lengthPerSegment);
                    if (pointIndex + 1u == activeCount) tipGuidance = climb;
                    predicted[pointIndex] += climb * surfaces.Controls.w * dt * dt;
                }
                previous[pointIndex] = position;
            }
            for (uint iteration = 0; iteration < pc.SolverIterations; ++iteration)
            {
                for (uint pointIndex = 1; pointIndex < activeCount; ++pointIndex)
                {
                    float3 delta = predicted[pointIndex] - predicted[pointIndex - 1];
                    float targetLength = clamp(grownLength - float(pointIndex - 1) * lengthPerSegment, 0.0, lengthPerSegment);
                    float distance = length(delta);
                    if (distance <= 0.000001 && targetLength > 0)
                    {
                        float3 extension = pointIndex > 1 ? predicted[pointIndex - 1] - predicted[pointIndex - 2] : direction;
                        uint surfaceIndex;
                        float3 closest, surfaceNormal;
                        if (surfaces.Controls.x > 0 && FindSurface(surfaces, predicted[pointIndex - 1], surfaceIndex, closest, surfaceNormal))
                            extension = SurfaceDirection(surfaces, surfaceNormal, extension);
                        predicted[pointIndex] = predicted[pointIndex - 1] + safeNormalize3(extension, direction) * targetLength;
                        previous[pointIndex] = predicted[pointIndex];
                        delta = predicted[pointIndex] - predicted[pointIndex - 1];
                        distance = length(delta);
                    }
                    if (distance > 0.000001)
                    {
                        float3 correction = delta / distance * (distance - targetLength) * pc.Stiffness;
                        bool firstFree = pointIndex > 1u && attached[pointIndex - 1u] == 0u;
                        bool secondFree = attached[pointIndex] == 0u && !(pinEnd && pointIndex == count - 1u);
                        float weight = firstFree && secondFree ? 0.5 : 1.0;
                        if (firstFree) predicted[pointIndex - 1] += correction * weight;
                        if (secondFree) predicted[pointIndex] -= correction * weight;
                    }
                }
                if (surfaces.Controls.x > 0)
                    for (uint pointIndex = 1; pointIndex < activeCount; ++pointIndex)
                    {
                        if (pinEnd && pointIndex == count - 1u) continue;
                        if (attached[pointIndex] != 0u) { predicted[pointIndex] = previous[pointIndex]; continue; }
                        uint surfaceIndex;
                        float3 closest, surfaceNormal;
                        if (FindSurface(surfaces, predicted[pointIndex], surfaceIndex, closest, surfaceNormal))
                            predicted[pointIndex] = closest + surfaceNormal * surfaces.Controls.z;
                    }
            }
            // Completed segments remain fixed in world space; the growing tip can slide.
            if (surfaces.Controls.x > 0)
                for (uint pointIndex = 1; pointIndex < activeCount; ++pointIndex)
                {
                    if ((pinEnd && pointIndex == count - 1u) || attached[pointIndex] != 0u || cooldown[pointIndex] > 0 || float(pointIndex) + 0.5 >= grownSegments) continue;
                    uint surfaceIndex;
                    float3 closest, surfaceNormal;
                    if (FindSurface(surfaces, predicted[pointIndex], surfaceIndex, closest, surfaceNormal))
                    {
                        predicted[pointIndex] = closest + surfaceNormal * surfaces.Controls.z;
                        previous[pointIndex] = predicted[pointIndex];
                        attached[pointIndex] = surfaceIndex + 1u;
                    }
                }
        }
        // The root remains pinned even when the timeline is paused.
        predicted[0] = anchor;
    }
    if (pinEnd)
    {
        predicted[0] = previous[0] = anchor;
        predicted[count - 1u] = previous[count - 1u] = endPosition;
        attached[count - 1u] = 0u;
        cooldown[count - 1u] = 0;
    }
    if (grownLength <= 0.000001)
    {
        predicted[0] = previous[0] = anchor;
        predicted[1] = previous[1] = anchor;
    }
    for (uint pointIndex = activeCount; pointIndex < count; ++pointIndex)
    {
        predicted[pointIndex] = predicted[activeCount - 1];
        previous[pointIndex] = predicted[pointIndex];
        attached[pointIndex] = 0u;
        cooldown[pointIndex] = 0;
    }
    if (surfaces.Controls.x == 0)
        tipGuidance = safeNormalize3(predicted[activeCount - 1u] - predicted[activeCount - 2u], direction);
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
        strandPoints[(base + pointIndex) * 3 + 1] = float4(pointIndex == 0u ? tipGuidance : previous[pointIndex], (pointIndex == 0u ? asfloat(pc.Generation) : (cooldown[pointIndex] > 0 ? -cooldown[pointIndex] : asfloat(attached[pointIndex]))));
        strandPoints[(base + pointIndex) * 3 + 2] = float4(normal, pointIndex == 2u && strandGrowth[pc.EmitterIndex].Controls.x > .5 ? age : (pointIndex == 1u ? (pc.GrowthSpeed > 0 ? max(age, 0.0) * pc.GrowthSpeed / pc.SegmentLength : float(count + 1u)) : grownSegments));
        tangent = nextTangent;
    }
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.StrandCount || pc.PointCount < 2 || pc.PointCount > 96) return;
    SGPUParticle root = particles[pc.ParticleOffset + id.x];
    uint base = pc.PointOffset + id.x * pc.PointCount * (1u + pc.BranchCount);
    SStrandGrowth development = strandGrowth[pc.EmitterIndex];
    if (development.Controls.x <= .5) development.Variation = float4(0,0,0,0);
    float growthTime = root.Age;
    if (development.Controls.x > .5) {
        bool fresh = asuint(strandPoints[base * 3u].w) != root.LinkOrder || asuint(strandPoints[base * 3u + 1u].w) != pc.Generation;
        growthTime = fresh ? 0.0 : max(0.0, strandPoints[(base + 2u) * 3u + 2u].w);
        if (development.Controls.y < .5 && TimeData.z < .5) growthTime += max(TimeData.x, 0.0);
        if (development.Controls.y > .5 && development.Controls.y < 1.5) growthTime = 1000000.0;
    }
    uint developmentSeed = GrowthHash(asuint(development.Controls.z) ^ root.LinkOrder ^ (id.x * 7919u));
    float seed = float(id.x) * 0.173 + float(root.LinkOrder % 65536) * 0.013;
    float3 direction = safeNormalize3(float3(hash31(float3(seed, 2, 3)) - 0.5,
        hash31(float3(seed, 4, 5)) - 0.5, hash31(float3(seed, 6, 7)) - 0.5), float3(0, 1, 0));
    float3 anchor = mul(pc.RootTransform, float4(root.Position, 1)).xyz;
    SStrandSurfaces surfaces = strandSurfaces[pc.EmitterIndex];
    uint surfaceIndex;
    float3 closest, surfaceNormal;
    if (surfaces.Controls.x > 0 && FindSurface(surfaces, anchor, surfaceIndex, closest, surfaceNormal))
        direction = SurfaceDirection(surfaces, surfaceNormal, surfaces.Direction.xyz);
    float4 endAnchor = strandEndAnchors[pc.EmitterIndex];
    bool pinEnd = endAnchor.w > .5;
    float3 endPosition = endAnchor.w > 1.5 ? endAnchor.xyz : mul(pc.RootTransform, float4(endAnchor.xyz, 1)).xyz;
    if (pinEnd) direction = safeNormalize3(endPosition - anchor, direction);
    SimulateChain(root, base, anchor, direction, growthTime, 1.0, id.x, pinEnd, endPosition);
    for (uint branch = 0; branch < pc.BranchCount; ++branch)
    {
        uint attachment = BranchNode(development, developmentSeed, branch, pc.BranchInterval, pc.PointCount);
        float3 branchAnchor = LoadPoint(base, attachment);
        float3 tangent = safeNormalize3(LoadPoint(base, attachment + 1u) - LoadPoint(base, attachment - 1u), direction);
        float3 side = strandPoints[(base + attachment) * 3u + 2u].xyz;
        float3 branchDirection = safeNormalize3(side * (branch % 2u == 0u ? 1.0 : -1.0) + tangent * 0.3, direction);
        float birth = pc.GrowthSpeed > 0 ? float(attachment + 1u) * pc.SegmentLength / pc.GrowthSpeed : 0.0;
        // A branch waits until the main tip passes its attachment, then grows independently.
        float age = growthTime - birth - BranchExtraDelay(development, developmentSeed, branch);
        if (pc.GrowthMaxLength > 0 && float(attachment + 1u) * pc.SegmentLength > pc.GrowthMaxLength) age = -1;
        SimulateChain(root, base + (branch + 1u) * pc.PointCount, branchAnchor, branchDirection,
            age, pc.BranchLengthScale, id.x + (branch + 1u) * 101u, false, float3(0, 0, 0));
    }
}
