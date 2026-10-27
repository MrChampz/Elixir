#ifndef AETHER_OPERATION_SAMPLEDISK_HLSL
#define AETHER_OPERATION_SAMPLEDISK_HLSL

static const uint Operation_SampleDisk = 2u;
static const uint SampleDisk_Center = 0u;
static const uint SampleDisk_Radius = 1u;
static const uint SampleDisk_Normal = 2u;

#include "RandomSampling.hlsl"

void SampleDisk(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float3 center = resolver.ReadVector3(SampleDisk_Center);
    float3 normal = resolver.ReadVector3(SampleDisk_Normal);
    float maxRadius = resolver.ReadScalar(SampleDisk_Radius);

    float radiusRandom = sqrt(Hash2(context.SeedBase + float2(8.63, 6.53)));
    float arcRandom = Hash2(context.SeedBase + float2(4.71, 1.29));

    float spawnAngle = arcRandom * 6.28318530718;
    float radius = maxRadius * radiusRandom;

    float3 right;
    float3 up;
    BuildBasis(normal, right, up);

    float3 direction = right * cos(spawnAngle) + up * sin(spawnAngle);

    float3 position = center + direction * radius;
    float4 positionValue = float4(position, 0.0);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        positionValue
    );

    float3 tangent = right * -sin(spawnAngle) + up * cos(spawnAngle);
    tangent = SafeNormalize(tangent, right);
    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        ParticleAttribute_Tangent,
        tangentValue
    );
}

#endif
