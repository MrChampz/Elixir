#ifndef AETHER_OPERATION_APPLYVORTEX_HLSL
#define AETHER_OPERATION_APPLYVORTEX_HLSL

static const uint Operation_ApplyVortex = 20u;
static const uint ApplyVortex_Center = 0u;
static const uint ApplyVortex_Normal = 1u;
static const uint ApplyVortex_Tangential = 2u;
static const uint ApplyVortex_Radial = 3u;

#include "OperationMath.hlsl"

void ApplyVortex(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float4 velocity = GetAttribute(context.Attributes, resolver.Operation.Target);
    float4 position = GetAttribute(context.Attributes, ParticleAttribute_Position);

    float3 center = resolver.ReadVector3(ApplyVortex_Center);
    float3 normal = SafeNormalize(resolver.ReadVector3(ApplyVortex_Normal), float3(0.0, 1.0, 0.0));
    float3 offset = position.xyz - center;

    float3 radial = SafeNormalize(offset, 0.0);
    float radialStrength = resolver.ReadScalar(ApplyVortex_Radial);

    float3 tangent = SafeNormalize(cross(normal, radial), 0.0);
    float tangentialStrength = resolver.ReadScalar(ApplyVortex_Tangential);

    velocity.xyz += (tangent * tangentialStrength + radial * radialStrength) * context.DeltaTime;

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        velocity
    );
    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        ParticleAttribute_Tangent,
        tangentValue
    );
}

#endif
