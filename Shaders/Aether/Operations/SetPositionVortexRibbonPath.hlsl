#ifndef AETHER_OPERATION_SETPOSITIONVORTEXRIBBONPATH_HLSL
#define AETHER_OPERATION_SETPOSITIONVORTEXRIBBONPATH_HLSL

static const uint Operation_SetPositionVortexRibbonPath = 12u;
static const uint SetPositionVortexRibbonPath_Center = 0u;
static const uint SetPositionVortexRibbonPath_OrbitSpeed = 1u;
static const uint SetPositionVortexRibbonPath_BaseRadius = 2u;
static const uint SetPositionVortexRibbonPath_RadiusAmplitude = 3u;
static const uint SetPositionVortexRibbonPath_RadiusSpeed = 4u;
static const uint SetPositionVortexRibbonPath_PulseAmplitude = 5u;
static const uint SetPositionVortexRibbonPath_PulseSpeed = 6u;
static const uint SetPositionVortexRibbonPath_CurlAmplitude = 7u;
static const uint SetPositionVortexRibbonPath_DepthAmplitude = 8u;

#include "OperationMath.hlsl"

struct VortexRibbonPathParams
{
    float3 Center;
    float  OrbitSpeed;
    float  BaseRadius;
    float  RadiusAmplitude;
    float  RadiusSpeed;
    float  PulseAmplitude;
    float  PulseSpeed;
    float  PulsePhase;
    float  CurlAmplitude;
    float2 CurlFrequency;
    float2 CurlPhase;
    float  DepthAmplitude;
    float  DepthFrequency;
    float  DepthPhase;
};

float3 VortexRibbonPath(float timeSeconds, VortexRibbonPathParams p)
{
    float phase = timeSeconds * p.OrbitSpeed;

    float radialPulse = p.PulseAmplitude * sin((timeSeconds * p.PulseSpeed) + p.PulsePhase);

    float radius =
        p.BaseRadius +
        (p.RadiusAmplitude * (0.5 + 0.5 * sin(timeSeconds * p.RadiusSpeed))) +
        radialPulse;

    float2 swirl = float2(cos(phase), sin(phase * 0.96)) * radius;

    float2 curl = float2(
        cos(phase * p.CurlFrequency.x + p.CurlPhase.x),
        sin(phase * p.CurlFrequency.y + p.CurlPhase.y)
    ) * p.CurlAmplitude;

    float z = p.Center.z + p.DepthAmplitude * sin(phase * p.DepthFrequency + p.DepthPhase);

    return float3(p.Center.xy + swirl + curl, z);
}

void SetPositionVortexRibbonPath(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    VortexRibbonPathParams params;
    params.Center = resolver.ReadVector3(SetPositionVortexRibbonPath_Center);

    params.OrbitSpeed = resolver.ReadScalar(SetPositionVortexRibbonPath_OrbitSpeed);
    params.BaseRadius = resolver.ReadScalar(SetPositionVortexRibbonPath_BaseRadius);
    params.RadiusAmplitude = resolver.ReadScalar(SetPositionVortexRibbonPath_RadiusAmplitude);
    params.RadiusSpeed = resolver.ReadScalar(SetPositionVortexRibbonPath_RadiusSpeed);

    params.PulseAmplitude = resolver.ReadScalar(SetPositionVortexRibbonPath_PulseAmplitude);
    params.PulseSpeed = resolver.ReadScalar(SetPositionVortexRibbonPath_PulseSpeed);
    params.PulsePhase = 0.0;
    params.CurlAmplitude = resolver.ReadScalar(SetPositionVortexRibbonPath_CurlAmplitude);
    params.DepthAmplitude = resolver.ReadScalar(SetPositionVortexRibbonPath_DepthAmplitude);

    params.CurlFrequency = float2(1.9, 1.7);
    params.CurlPhase = float2(0.6, -0.3);
    params.DepthFrequency = 0.73;
    params.DepthPhase = 0.4;

    float spawnRate = max(context.SpawnRate, 0.001);
    float timeOffset = float((context.SpawnCount - 1u) - context.SpawnOrder) / spawnRate;
    float sampleTime = context.Time - timeOffset;

    float3 position = VortexRibbonPath(sampleTime, params);
    float4 positionValue = float4(position.xyz, 1.0);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        positionValue
    );
    float4 velocityValue = 0.0;

    SetAttribute(
        context.Attributes,
        ParticleAttribute_Velocity,
        velocityValue
    );

    float tangentStep = max(1.0 / spawnRate, 0.001);
    float3 prevPosition = VortexRibbonPath(sampleTime - tangentStep, params);
    float3 nextPosition = VortexRibbonPath(sampleTime + tangentStep, params);
    float3 tangent = SafeNormalize(nextPosition - prevPosition, float3(1.0, 0.0, 0.0));

    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        ParticleAttribute_Tangent,
        tangentValue
    );
}

#endif
