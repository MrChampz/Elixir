#ifndef AETHER_OPERATION_CLAMP_HLSL
#define AETHER_OPERATION_CLAMP_HLSL

static const uint Operation_Clamp = 18u;
static const uint Clamp_Minimum = 0u;
static const uint Clamp_Maximum = 1u;

void Clamp(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = GetAttribute(context.Attributes, executor.Reader.Operation.Target);
    float4 minValue = executor.Reader.ReadValue(Clamp_Minimum);
    float4 maxValue = executor.Reader.ReadValue(Clamp_Maximum);
    value = clamp(value, minValue, maxValue);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
