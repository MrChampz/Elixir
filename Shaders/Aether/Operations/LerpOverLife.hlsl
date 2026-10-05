#ifndef AETHER_OPERATION_LERPOVERLIFE_HLSL
#define AETHER_OPERATION_LERPOVERLIFE_HLSL

static const uint Operation_LerpOverLife = 7u;
static const uint LerpOverLife_Start = 0u;
static const uint LerpOverLife_End = 1u;

void LerpOverLife(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value0 = executor.Reader.ReadValue(LerpOverLife_Start);
    float4 value1 = executor.Reader.ReadValue(LerpOverLife_End);
    float4 value = lerp(value0, value1, context.NormalizedAge);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );

}

#endif
