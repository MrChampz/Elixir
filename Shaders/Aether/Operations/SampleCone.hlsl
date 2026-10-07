#ifndef AETHER_OPERATION_SAMPLECONE_HLSL
#define AETHER_OPERATION_SAMPLECONE_HLSL

static const uint Operation_SampleCone = 3u;
static const uint SampleCone_Direction = 0u;
static const uint SampleCone_Angle = 1u;
static const uint SampleCone_MinSpeed = 2u;
static const uint SampleCone_MaxSpeed = 3u;

#include "RandomSampling.hlsl"

void SampleCone(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float3 axis = SafeNormalize(executor.Reader.ReadVector3(SampleCone_Direction), float3(0.0, 1.0, 0.0));
    float angle = executor.Reader.ReadScalar(SampleCone_Angle) * 0.5;
    float minSpeed = executor.Reader.ReadScalar(SampleCone_MinSpeed);
    float maxSpeed = executor.Reader.ReadScalar(SampleCone_MaxSpeed);

    float angleRandom = Hash2(context.SeedBase + float2(3.17, 9.41));
    float speedRandom = Hash2(context.SeedBase + float2(5.23, 2.19));
    float coneRandom = Hash2(context.SeedBase + float2(8.41, 4.77));

    float speed = lerp(minSpeed, maxSpeed, speedRandom);

    float cosTheta = lerp(1.0, cos(angle), coneRandom);
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = angleRandom * 6.28318530718;

    float3 right;
    float3 up;
    BuildBasis(axis, right, up);

    float3 direction = right * (cos(phi) * sinTheta) +
                       up * (sin(phi) * sinTheta) +
                       axis * cosTheta;
    direction = SafeNormalize(direction, axis);

    float3 velocity = direction * speed;
    float4 velocityValue = float4(velocity, 0.0);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        velocityValue
    );

    float3 tangent = SafeNormalize(velocity, GetAttribute(context.Attributes, 6u).xyz);
    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        8u,
        tangentValue
    );
}

#endif
