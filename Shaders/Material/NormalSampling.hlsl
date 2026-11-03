#ifndef ELIXIR_MATERIAL_NORMAL_SAMPLING_HLSL
#define ELIXIR_MATERIAL_NORMAL_SAMPLING_HLSL

// Decode a tangent-space normal independently of texture sampling.
float3 DecodeNormal(float3 encoded)
{
    return normalize(encoded * 2.0f - 1.0f);
}

// Preserve alpha when the graph requests RGBA or the alpha channel.
float4 DecodeNormal(float4 encoded)
{
    float3 normal = DecodeNormal(encoded.xyz);
    return float4(normal, encoded.a);
}

#endif
