#ifndef AETHER_MATH_HLSL
#define AETHER_MATH_HLSL

float3 SafeNormalize(float3 value, float3 fallback) {
  float lengthSquared = dot(value, value);
  if (lengthSquared < 0.000001) {
    return fallback;
  }
  return value * rsqrt(lengthSquared);
}

// Rodrigues rotation of v around a unit axis by angle
float3 RotateAroundAxis(float3 v, float3 axis, float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return (v * c) + (cross(axis, v) * s) + (axis * dot(axis, v) * (1.0 - c));
}

#endif
