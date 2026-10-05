#ifndef AETHER_OPERATION_ADD_HLSL
#define AETHER_OPERATION_ADD_HLSL

static const uint Operation_Add = 16u;
static const uint Add_Value = 0u;

void Add(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = GetAttribute(context.Attributes, executor.Reader.Operation.Target);
    value += executor.Reader.ReadValue(Add_Value);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );

}

#endif
