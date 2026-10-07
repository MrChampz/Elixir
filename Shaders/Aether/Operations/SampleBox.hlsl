#ifndef AETHER_OPERATION_SAMPLEBOX_HLSL
#define AETHER_OPERATION_SAMPLEBOX_HLSL

static const uint Operation_SampleBox = 4u;
static const uint SampleBox_Minimum = 0u;
static const uint SampleBox_Maximum = 1u;

#include "RandomSampling.hlsl"

void SampleBox(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float3 minBounds = executor.Reader.ReadVector3(SampleBox_Minimum);
    float3 maxBounds = executor.Reader.ReadVector3(SampleBox_Maximum);

    float2 seed = context.SeedBase + float2(float(context.OperationIndex) * 1.3, 5.7);
    float3 random = RandomVector(seed).xyz;

    float3 position = lerp(minBounds, maxBounds, random);
    float4 positionValue = float4(position, 0.0);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        positionValue
    );
}

#endif
