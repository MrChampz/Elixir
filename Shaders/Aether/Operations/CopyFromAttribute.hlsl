#ifndef AETHER_OPERATION_COPYFROMATTRIBUTE_HLSL
#define AETHER_OPERATION_COPYFROMATTRIBUTE_HLSL

static const uint Operation_CopyFromAttribute = 19u;
static const uint CopyFromAttribute_Value = 0u;

void CopyFromAttribute(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    if (resolver.Operation.Target == ParticleAttribute_RibbonId)
    {
        uint value = resolver.ReadUInt(CopyFromAttribute_Value);

        SetAttribute(
            context.Attributes,
            resolver.Operation.Target,
            value
        );
        return;
    }

    float4 value = resolver.ReadValue(CopyFromAttribute_Value);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
