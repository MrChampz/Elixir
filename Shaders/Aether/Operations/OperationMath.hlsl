#ifndef AETHER_OPERATION_MATH_HLSL
#define AETHER_OPERATION_MATH_HLSL

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared < 0.000001)
        return fallback;

    return value * rsqrt(lengthSquared);
}

#endif
