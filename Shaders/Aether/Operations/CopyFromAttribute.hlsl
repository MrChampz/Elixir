#ifndef AETHER_OPERATION_COPYFROMATTRIBUTE_HLSL
#define AETHER_OPERATION_COPYFROMATTRIBUTE_HLSL

static const uint Operation_CopyFromAttribute = 19u;
static const uint CopyFromAttribute_Value = 0u;

void CopyFromAttribute(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    if (executor.Reader.Operation.Target == 9u)
    {
        uint value = executor.Reader.ReadUInt(CopyFromAttribute_Value);

        SetAttribute(
            context.Attributes,
            executor.Reader.Operation.Target,
            value
        );
        return;
    }

    float4 value = executor.Reader.ReadValue(CopyFromAttribute_Value);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
