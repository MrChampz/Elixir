#ifndef AETHER_OPERATION_APPLY_CURL_NOISE_HLSL
#define AETHER_OPERATION_APPLY_CURL_NOISE_HLSL
#include "../StrandMath.hlsl"
static const uint Operation_ApplyCurlNoise = 21u;

void ApplyCurlNoise(inout SOperationExecutor executor, SOperandResolver resolver,
    inout SOperationExecutorContext context)
{
    float strength = max(resolver.ReadScalar(0), 0.0);
    float frequency = max(resolver.ReadScalar(1), 0.000001);
    float3 scrollVelocity = resolver.ReadVector3(2);
    uint seedBits = resolver.ReadUInt(3);
    float seed = context.DynamicInputs.ParticleSeed + float(seedBits & 65535u) + float(seedBits >> 16u) * 0.017;
    float3 position = GetAttribute(context.Attributes, ParticleAttribute_Position).xyz;
    float3 samplePosition = position * frequency + float3(seed * 37, seed * 21, seed * 11) + context.Time * scrollVelocity;
    float3 acceleration = curlNoise3(samplePosition) * strength;
    float4 velocity = GetAttribute(context.Attributes, ParticleAttribute_Velocity);
    velocity.xyz += acceleration * context.DeltaTime;
    SetAttribute(context.Attributes, resolver.Operation.Target, velocity);
}
#endif
