#ifndef AETHER_NOISE_HLSL
#define AETHER_NOISE_HLSL

float Hash3(float3 p) {
    p = frac(p * float3(123.34, 456.21, 789.92));
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3(float3 p) {
    float3 cell = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - (2.0 * f));

    float n000 = Hash3(cell);
    float n100 = Hash3(cell + float3(1.0, 0.0, 0.0));
    float n010 = Hash3(cell + float3(0.0, 1.0, 0.0));
    float n110 = Hash3(cell + float3(1.0, 1.0, 0.0));
    float n001 = Hash3(cell + float3(0.0, 0.0, 1.0));
    float n101 = Hash3(cell + float3(1.0, 0.0, 1.0));
    float n011 = Hash3(cell + float3(0.0, 1.0, 1.0));
    float n111 = Hash3(cell + float3(1.0, 1.0, 1.0));

    float bottom = lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y);
    float top = lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y);

    return lerp(bottom, top, u.z);
}

// vector potential for divergence-free 3D curl noise
float3 NoisePotential3(float3 p) {
    return float3(
        ValueNoise3(p) + (0.5 * ValueNoise3((p * 2.03) + float3(17.7, 9.3, 3.1))),
        ValueNoise3(p + float3(31.4, 5.2, 12.9)),
        ValueNoise3(p + float3(7.7, 24.1, 51.3))
    );
}

float3 CurlNoise3(float3 p) {
    const float epsilon = 0.08;

    float3 dx = (NoisePotential3(p + float3(epsilon, 0.0, 0.0)) -
        NoisePotential3(p - float3(epsilon, 0.0, 0.0))) / (2.0 * epsilon);
    float3 dy = (NoisePotential3(p + float3(0.0, epsilon, 0.0)) -
        NoisePotential3(p - float3(0.0, epsilon, 0.0))) / (2.0 * epsilon);
    float3 dz = (NoisePotential3(p + float3(0.0, 0.0, epsilon)) -
        NoisePotential3(p - float3(0.0, 0.0, epsilon))) / (2.0 * epsilon);

    return float3(dy.z - dz.y, dz.x - dx.z, dx.y - dy.x);
}

#endif
