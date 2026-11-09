#ifndef AETHER_OPERATION_SAMPLE_CURL_NOISE_HLSL
#define AETHER_OPERATION_SAMPLE_CURL_NOISE_HLSL

#include "../Noise.hlsl"

static const uint Operation_SampleCurlNoise = 21u;
static const uint SampleCurlNoise_Position = 0u;
static const uint SampleCurlNoise_Seed = 1u;
static const uint SampleCurlNoise_SeedOffset = 2u;

void SampleCurlNoise(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    float3 position = resolver.ReadVector3(SampleCurlNoise_Position);
    uint seedBits = resolver.ReadUInt(SampleCurlNoise_Seed);
    float seed = resolver.ReadScalar(SampleCurlNoise_SeedOffset) +
        float(seedBits & 65535u) + float(seedBits >> 16u) * 0.017;
    float3 samplePosition = position + float3(seed * 37, seed * 21, seed * 11);
    float3 value = CurlNoise3(samplePosition);
    SetAttribute(context.Attributes, resolver.Operation.Target, float4(value, 0.0));
}

#endif
