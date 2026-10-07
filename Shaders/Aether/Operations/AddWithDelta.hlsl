#ifndef AETHER_OPERATION_ADDWITHDELTA_HLSL
#define AETHER_OPERATION_ADDWITHDELTA_HLSL

static const uint Operation_AddWithDelta = 5u;
static const uint AddWithDelta_Value = 0u;
static const uint AddWithDelta_Multiplier = 1u;

void AddWithDelta(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = GetAttribute(context.Attributes, executor.Reader.Operation.Target);
    float inputMultiplier = executor.Reader.ReadScalar(AddWithDelta_Multiplier);

    value += executor.Reader.ReadValue(AddWithDelta_Value) * (context.DeltaTime * inputMultiplier);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
