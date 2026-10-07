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
        parameters[baseParameterIndex + 0].Value,
        parameters[baseParameterIndex + 1].Value,
        parameters[baseParameterIndex + 2].Value,
        parameters[baseParameterIndex + 3].Value,
        parameters[baseParameterIndex + 4].Value,
        parameters[baseParameterIndex + 5].Value,
        parameters[baseParameterIndex + 6].Value,
        parameters[baseParameterIndex + 7].Value
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
