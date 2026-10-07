#ifndef AETHER_OPERATION_ADDFROMATTRIBUTE_HLSL
#define AETHER_OPERATION_ADDFROMATTRIBUTE_HLSL

static const uint Operation_AddFromAttribute = 9u;
static const uint AddFromAttribute_Value = 0u;
static const uint AddFromAttribute_Scale = 1u;

void AddFromAttribute(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = GetAttribute(context.Attributes, executor.Reader.Operation.Target);
    float4 sourceValue = executor.Reader.ReadValue(AddFromAttribute_Value);
    value += sourceValue * (executor.Reader.ReadScalar(AddFromAttribute_Scale) * context.DeltaTime);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
