#ifndef AETHER_OPERATION_SETLITERAL_HLSL
#define AETHER_OPERATION_SETLITERAL_HLSL

static const uint Operation_SetLiteral = 0u;
static const uint SetLiteral_Value = 0u;

void SetLiteral(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    if (resolver.Operation.Target == ParticleAttribute_RibbonId)
    {
        uint value = resolver.ReadUInt(SetLiteral_Value);

        SetAttribute(
            context.Attributes,
            resolver.Operation.Target,
            value
        );
        return;
    }

    float4 value = resolver.ReadValue(SetLiteral_Value);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
