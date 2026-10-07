#ifndef AETHER_OPERATION_SAMPLECONE_HLSL
#define AETHER_OPERATION_SAMPLECONE_HLSL

static const uint Operation_SampleCone = 3u;
static const uint SampleCone_Direction = 0u;
static const uint SampleCone_Angle = 1u;
static const uint SampleCone_MinSpeed = 2u;
static const uint SampleCone_MaxSpeed = 3u;

#include "RandomSampling.hlsl"

void SampleCone(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float3 axis = SafeNormalize(resolver.ReadVector3(SampleCone_Direction), float3(0.0, 1.0, 0.0));
    float angle = resolver.ReadScalar(SampleCone_Angle) * 0.5;
    float minSpeed = resolver.ReadScalar(SampleCone_MinSpeed);
    float maxSpeed = resolver.ReadScalar(SampleCone_MaxSpeed);

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
        resolver.Operation.Target,
        velocityValue
    );

    float3 tangent = SafeNormalize(velocity, GetAttribute(context.Attributes, ParticleAttribute_Size).xyz);
    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        ParticleAttribute_Tangent,
        tangentValue
    );
}

#endif
