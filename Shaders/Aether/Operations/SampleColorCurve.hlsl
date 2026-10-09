#ifndef AETHER_OPERATION_SAMPLECOLORCURVE_HLSL
#define AETHER_OPERATION_SAMPLECOLORCURVE_HLSL

static const uint Operation_SampleColorCurve = 15u;
static const uint SampleColorCurve_Curve = 0u;
static const uint SampleColorCurve_Input = 1u;

float4 SampleColorCurve(int baseParameterIndex, float t)
{
    if (baseParameterIndex < 0)
        return 1.0;

    float4 samples[8] =
    {
        asfloat(parameters.Load4(uint(baseParameterIndex + 0)  * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 4)  * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 8)  * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 12) * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 16) * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 20) * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 24) * 4u)),
        asfloat(parameters.Load4(uint(baseParameterIndex + 28) * 4u))
    };

    float clampedT = clamp(t, 0.0, 1.0);
    float samplePosition = clampedT * 7.0;

    uint lowerIndex = (uint)floor(samplePosition);
    uint upperIndex = min(lowerIndex + 1u, 7u);
    float fraction = frac(samplePosition);

    return lerp(samples[lowerIndex], samples[upperIndex], fraction);
}

void SampleColorCurve(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float inputValue = resolver.ReadScalar(SampleColorCurve_Input);
    float4 value = SampleColorCurve(resolver.ReadCurve(SampleColorCurve_Curve), inputValue);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        value
    );
}

#endif
