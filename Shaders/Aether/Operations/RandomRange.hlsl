#ifndef AETHER_OPERATION_RANDOMRANGE_HLSL
#define AETHER_OPERATION_RANDOMRANGE_HLSL

static const uint Operation_RandomRange = 1u;
static const uint RandomRange_Minimum = 0u;
static const uint RandomRange_Maximum = 1u;

#include "RandomSampling.hlsl"

void RandomRange(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float2 seed = context.SeedBase + float2(float(context.OperationIndex) * 1.7, float(executor.Reader.Operation.Target) * 2.3);
    float4 randomValue = RandomVector(seed);
    float4 value0 = executor.Reader.ReadValue(RandomRange_Minimum);
    float4 value1 = executor.Reader.ReadValue(RandomRange_Maximum);

    float4 value = lerp(value0, value1, randomValue);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
