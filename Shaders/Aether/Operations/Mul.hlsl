#ifndef AETHER_OPERATION_MUL_HLSL
#define AETHER_OPERATION_MUL_HLSL

static const uint Operation_Mul = 17u;
static const uint Mul_Value = 0u;

void Mul(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float4 value = GetAttribute(context.Attributes, resolver.Operation.Target);
    float4 multiplier = resolver.ReadValue(Mul_Value);

    // Scalar operands scale every component; vector operands multiply component by component.
    uint format = GetOperandFormat(resolver.GetOperandReference(Mul_Value));

    if (format == OperandFormat_Float)
        multiplier = multiplier.xxxx;

    value *= multiplier;

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
