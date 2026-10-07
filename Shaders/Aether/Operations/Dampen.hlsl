#ifndef AETHER_OPERATION_DAMPEN_HLSL
#define AETHER_OPERATION_DAMPEN_HLSL

static const uint Operation_Dampen = 6u;
static const uint Dampen_Drag = 0u;

void Dampen(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 value = GetAttribute(context.Attributes, executor.Reader.Operation.Target);
    float drag = executor.Reader.ReadScalar(Dampen_Drag);
    value *= max(0.0, 1.0 - drag * context.DeltaTime);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        value
    );
}

#endif
