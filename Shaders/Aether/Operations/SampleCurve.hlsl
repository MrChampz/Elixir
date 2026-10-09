#ifndef AETHER_OPERATION_SAMPLECURVE_HLSL
#define AETHER_OPERATION_SAMPLECURVE_HLSL

static const uint Operation_SampleCurve = 14u;
static const uint SampleCurve_Curve = 0u;
static const uint SampleCurve_Input = 1u;

float SampleCurve(int baseParameterIndex, float t)
{
    if (baseParameterIndex < 0)
        return 0.0;

    float4 a = asfloat(parameters.Load4(uint(baseParameterIndex) * 4u));
    float4 b = asfloat(parameters.Load4(uint(baseParameterIndex + 4) * 4u));

    float samples[8] = { a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w };

    float clampedT = clamp(t, 0.0, 1.0);
    float samplePosition = clampedT * 7.0;

    uint lowerIndex = (uint)floor(samplePosition);
    uint upperIndex = min(lowerIndex + 1u, 7u);
    float fraction = frac(samplePosition);

    return lerp(samples[lowerIndex], samples[upperIndex], fraction);
}

void SampleCurve(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float inputValue = resolver.ReadScalar(SampleCurve_Input);
    float value = SampleCurve(resolver.ReadCurve(SampleCurve_Curve), inputValue);
    float4 attributeValue = float4(value, 0.0, 0.0, 0.0);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        attributeValue
    );
}

#endif
