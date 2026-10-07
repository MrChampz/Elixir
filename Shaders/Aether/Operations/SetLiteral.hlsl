#ifndef AETHER_OPERATION_SETLITERAL_HLSL
#define AETHER_OPERATION_SETLITERAL_HLSL

static const uint Operation_SetLiteral = 0u;
static const uint SetLiteral_Value = 0u;

void SetLiteral(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    if (executor.Reader.Operation.Target == 9u)
    {
        uint value = executor.Reader.ReadUInt(SetLiteral_Value);

        SetAttribute(
            context.Attributes,
            executor.Reader.Operation.Target,
            value
        );
        return;
    }

    float4 value = executor.Reader.ReadValue(SetLiteral_Value);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
