#ifndef AETHER_RANDOM_SAMPLING_HLSL
#define AETHER_RANDOM_SAMPLING_HLSL

#include "OperationMath.hlsl"

float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float4 RandomVector(float2 seed)
{
    return float4(
        Hash2(seed + float2(1.31, 2.17)),
        Hash2(seed + float2(3.11, 4.29)),
        Hash2(seed + float2(5.71, 6.13)),
        Hash2(seed + float2(7.43, 8.59))
    );
}

void BuildBasis(float3 axis, out float3 right, out float3 up)
{
    float3 helper = abs(axis.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    right = SafeNormalize(cross(helper, axis), float3(1.0, 0.0, 0.0));
    up = cross(axis, right);
}

#endif
