#ifndef ELIXIR_MATERIAL_PARTICLE_LIGHTING_HLSL
#define ELIXIR_MATERIAL_PARTICLE_LIGHTING_HLSL

#include "../Material/DirectLighting.hlsl"
#include "../Material/ShadingModels.hlsl"

float3 EvaluateParticleLighting(
    float3 baseColor,
    float3 emissive,
    float3 normal,
    float3 viewDirection,
    float roughness,
    float metallic,
    float3 lightDirection,
    float4 lightColor
)
{
#if MATERIAL_SHADING_MODEL == MATERIAL_SHADING_MODEL_UNLIT
    return baseColor + emissive;
#elif MATERIAL_SHADING_MODEL == MATERIAL_SHADING_MODEL_LIT
    const float filteredRoughness = FilterSpecularRoughness(
        clamp(roughness, 0.045f, 1.0f), normal
    );
    const SDirectLighting direct = EvaluateDirectLighting(
        baseColor,
        normal,
        viewDirection,
        filteredRoughness,
        metallic,
        lerp(0.04f.xxx, baseColor, metallic),
        normalize(lightDirection),
        lightColor.rgb * lightColor.a,
        1.0f
    );
    return direct.Diffuse + direct.Specular + emissive;
#else
#error Particle lighting supports only Unlit and Lit materials.
#endif
}

#endif
