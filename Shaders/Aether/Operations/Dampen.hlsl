#ifndef AETHER_OPERATION_DAMPEN_HLSL
#define AETHER_OPERATION_DAMPEN_HLSL

static const uint Operation_Dampen = 6u;
static const uint Dampen_Drag = 0u;

void Dampen(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float4 value = GetAttribute(context.Attributes, resolver.Operation.Target);
    float drag = resolver.ReadScalar(Dampen_Drag);
    value *= max(0.0, 1.0 - drag * context.DeltaTime);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
