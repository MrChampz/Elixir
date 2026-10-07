#ifndef AETHER_OPERATION_SETPOSITIONCIRCULARPATH_HLSL
#define AETHER_OPERATION_SETPOSITIONCIRCULARPATH_HLSL

static const uint Operation_SetPositionCircularPath = 11u;
static const uint SetPositionCircularPath_BaseOffset = 0u;
static const uint SetPositionCircularPath_TimeScale = 1u;
static const uint SetPositionCircularPath_PrimaryAmplitude = 2u;
static const uint SetPositionCircularPath_SecondaryAmplitude = 3u;

#include "OperationMath.hlsl"

void SetPositionCircularPath(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float3 baseOffset = executor.Reader.ReadVector3(SetPositionCircularPath_BaseOffset);
    float3 primaryAmplitude = executor.Reader.ReadVector3(SetPositionCircularPath_PrimaryAmplitude);
    float3 secondaryAmplitude = executor.Reader.ReadVector3(SetPositionCircularPath_SecondaryAmplitude);
    float timeScale = executor.Reader.ReadScalar(SetPositionCircularPath_TimeScale);

    float spawnRate = max(context.SpawnRate, 0.001);
    float timeOffset = float((context.SpawnCount - 1u) - context.SpawnOrder) / spawnRate;
    float sampleTime = context.Time - timeOffset;

    float phase = sampleTime * timeScale;

    float x = baseOffset.x + (primaryAmplitude.x * sin(phase)) + (secondaryAmplitude.x * sin(phase * 2.1));
    float y = baseOffset.y + (primaryAmplitude.y * sin((phase * 0.58) + 0.65)) +
        (secondaryAmplitude.y * cos((phase * 1.34) - 0.4));
    float z = baseOffset.z + (primaryAmplitude.z * cos((phase * 0.72) + 0.35)) +
        (secondaryAmplitude.z * sin((phase * 1.61) - 0.2));

    float4 positionValue = float4(x, y, z, 1.0);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        positionValue
    );
    float4 velocityValue = 0.0;

    SetAttribute(
        context.Attributes,
        4u,
        velocityValue
    );

    float dx = primaryAmplitude.x * cos(phase) + secondaryAmplitude.x * 2.1 * cos(phase * 2.1);
    float dy = primaryAmplitude.y * 0.58 * cos(phase * 0.58 + 0.65) - secondaryAmplitude.y * 1.34 * sin(phase * 1.34 - 0.4);
    float dz = -primaryAmplitude.z * 0.72 * sin((phase * 0.72) + 0.35) + secondaryAmplitude.z * 1.61 * cos((phase * 1.61) - 0.2);
    float3 tangent = SafeNormalize(float3(dx, dy, dz), float3(1.0, 0.0, 0.0));

    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        8u,
        tangentValue
    );
}

#endif
