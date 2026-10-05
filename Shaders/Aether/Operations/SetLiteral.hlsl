#ifndef AETHER_OPERATION_SETLITERAL_HLSL
#define AETHER_OPERATION_SETLITERAL_HLSL

static const uint Operation_SetLiteral = 0u;
static const uint SetLiteral_Value = 0u;

void SetLiteral(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = executor.Reader.Operation.Target == 9u
       ? float4(float(executor.Reader.ReadUInt(SetLiteral_Value)), 0.0, 0.0, 0.0)
       : executor.Reader.ReadValue(SetLiteral_Value);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
