#include "../../../Shaders/Material/DirectLighting.hlsl"

[[vk::binding(0, 0)]] RWStructuredBuffer<float4> results;

[numthreads(1, 1, 1)]
void main()
{
    const float3 front = float3(0, 0, 1);
    const float3 base = float3(1, 1, 1);
    const float3 reflectance = float3(0.04, 0.04, 0.04);
    const SDirectLighting lit = EvaluateDirectLighting(
        base, front, front, 0.5, 0, reflectance, front, base, 1
    );
    results[0] = float4(lit.Diffuse, 1);
    results[1] = float4(lit.Specular, 1);
    const SDirectLighting back = EvaluateDirectLighting(
        base, -front, front, 0.5, 0, reflectance, front, base, 1
    );
    results[2] = float4(back.Diffuse + back.Specular, 1);
    const SDirectLighting dark = EvaluateDirectLighting(
        base, front, front, 0.5, 0, reflectance, front, 0.0.xxx, 1
    );
    results[3] = float4(dark.Diffuse + dark.Specular, 1);
}
