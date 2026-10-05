#ifndef AETHER_OPERATION_KILLOUTSIDEBOUNDS_HLSL
#define AETHER_OPERATION_KILLOUTSIDEBOUNDS_HLSL

static const uint Operation_KillOutsideBounds = 8u;
static const uint KillOutsideBounds_Minimum = 0u;
static const uint KillOutsideBounds_Maximum = 1u;

void KillOutsideBounds(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float4 position = GetAttribute(context.Attributes, 1u);
    float3 minimum = executor.Reader.ReadVector3(KillOutsideBounds_Minimum);
    float3 maximum = executor.Reader.ReadVector3(KillOutsideBounds_Maximum);
    bool outsideBounds = any(position.xyz < minimum) || any(position.xyz > maximum);
    context.Kill = context.Kill || outsideBounds;

}

#endif
