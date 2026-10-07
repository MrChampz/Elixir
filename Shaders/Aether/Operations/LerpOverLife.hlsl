#ifndef AETHER_OPERATION_LERPOVERLIFE_HLSL
#define AETHER_OPERATION_LERPOVERLIFE_HLSL

static const uint Operation_LerpOverLife = 7u;
static const uint LerpOverLife_Start = 0u;
static const uint LerpOverLife_End = 1u;

void LerpOverLife(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float4 value0 = resolver.ReadValue(LerpOverLife_Start);
    float4 value1 = resolver.ReadValue(LerpOverLife_End);
    float4 value = lerp(value0, value1, context.NormalizedAge);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
