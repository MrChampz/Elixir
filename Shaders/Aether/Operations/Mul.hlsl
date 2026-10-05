#ifndef AETHER_OPERATION_MUL_HLSL
#define AETHER_OPERATION_MUL_HLSL

static const uint Operation_Mul = 17u;
static const uint Mul_Value = 0u;

void Mul(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = GetAttribute(context.Attributes, executor.Reader.Operation.Target);
    value *= executor.Reader.ReadValue(Mul_Value);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );

}

#endif
