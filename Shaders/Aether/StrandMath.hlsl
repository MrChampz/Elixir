#ifndef AETHER_STRAND_MATH_HLSL
#define AETHER_STRAND_MATH_HLSL

float3 safeNormalize3(float3 value, float3 fallback) {
  float lengthSquared = dot(value, value);
  if (lengthSquared < 0.000001) {
    return fallback;
  }
  return value * rsqrt(lengthSquared);
}

// Rodrigues rotation of v around a unit axis by angle
float3 rotateAroundAxis(float3 v, float3 axis, float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return (v * c) + (cross(axis, v) * s) + (axis * dot(axis, v) * (1.0 - c));
}

float hash31(float3 p) {
  p = frac(p * float3(123.34, 456.21, 789.92));
  p += dot(p, p.yzx + 33.33);
  return frac((p.x + p.y) * p.z);
}

float valueNoise3(float3 p) {
  float3 cell = floor(p);
  float3 f = frac(p);
  float3 u = f * f * (3.0 - (2.0 * f));
  float n000 = hash31(cell);
  float n100 = hash31(cell + float3(1.0, 0.0, 0.0));
  float n010 = hash31(cell + float3(0.0, 1.0, 0.0));
  float n110 = hash31(cell + float3(1.0, 1.0, 0.0));
  float n001 = hash31(cell + float3(0.0, 0.0, 1.0));
  float n101 = hash31(cell + float3(1.0, 0.0, 1.0));
  float n011 = hash31(cell + float3(0.0, 1.0, 1.0));
  float n111 = hash31(cell + float3(1.0, 1.0, 1.0));
  float bottom = lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y);
  float top = lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y);
  return lerp(bottom, top, u.z);
}

// vector potential for divergence-free 3D curl noise
float3 noisePotential3(float3 p) {
  return float3(
      valueNoise3(p) + (0.5 * valueNoise3((p * 2.03) + float3(17.7, 9.3, 3.1))),
      valueNoise3(p + float3(31.4, 5.2, 12.9)),
      valueNoise3(p + float3(7.7, 24.1, 51.3)));
}

float3 curlNoise3(float3 p) {
  const float epsilon = 0.08;
  float3 dx = (noisePotential3(p + float3(epsilon, 0.0, 0.0)) - noisePotential3(p - float3(epsilon, 0.0, 0.0))) /
            (2.0 * epsilon);
  float3 dy = (noisePotential3(p + float3(0.0, epsilon, 0.0)) - noisePotential3(p - float3(0.0, epsilon, 0.0))) /
            (2.0 * epsilon);
  float3 dz = (noisePotential3(p + float3(0.0, 0.0, epsilon)) - noisePotential3(p - float3(0.0, 0.0, epsilon))) /
            (2.0 * epsilon);
  return float3(dy.z - dz.y, dz.x - dx.z, dx.y - dy.x);
}


#endif
