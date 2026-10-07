#ifndef AETHER_OPERATION_ADDWITHDELTA_HLSL
#define AETHER_OPERATION_ADDWITHDELTA_HLSL

static const uint Operation_AddWithDelta = 5u;
static const uint AddWithDelta_Value = 0u;
static const uint AddWithDelta_Multiplier = 1u;

void AddWithDelta(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float4 value = GetAttribute(context.Attributes, resolver.Operation.Target);
    float inputMultiplier = resolver.ReadScalar(AddWithDelta_Multiplier);

    value += resolver.ReadValue(AddWithDelta_Value) * (context.DeltaTime * inputMultiplier);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
