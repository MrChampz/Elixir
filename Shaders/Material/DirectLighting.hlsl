#ifndef ELIXIR_MATERIAL_DIRECT_LIGHTING_HLSL
#define ELIXIR_MATERIAL_DIRECT_LIGHTING_HLSL

float DistributionGGX(float NdotH, float roughness)
{
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float denominator = NdotH * NdotH * (alphaSquared - 1.0f) + 1.0f;
    return alphaSquared / max(3.14159265359f * denominator * denominator, 1e-7f);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    const float roughnessPlusOne = roughness + 1.0f;
    const float k = (roughnessPlusOne * roughnessPlusOne) * 0.125f;
    return NdotX / max(NdotX * (1.0f - k) + k, 1e-7f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float VdotH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - VdotH), 5.0f);
}

/**
 * Filters specular highlights where the final shading normal changes sharply
 * between adjacent screen pixels.
 */
float FilterSpecularRoughness(float roughness, float3 normal)
{
    const float3 ndx = ddx(normal);
    const float3 ndy = ddy(normal);
    const float variance = max(dot(ndx, ndx), dot(ndy, ndy));
    return sqrt(saturate(roughness * roughness + variance));
}

// Contributions from the shared Cook-Torrance directional-light evaluation.
struct SDirectLighting
{
    float3 Diffuse;
    float3 Specular;
};

SDirectLighting EvaluateDirectLighting(
    float3 baseColor,
    float3 normal,
    float3 viewDirection,
    float roughness,
    float metallic,
    float3 reflectance,
    float3 lightDirection,
    float3 radiance,
    float occlusion
)
{
    SDirectLighting result = (SDirectLighting)0;
    const float NdotL = saturate(dot(normal, lightDirection));
    if (NdotL <= 0.0f) return result;

    const float NdotV = saturate(dot(normal, viewDirection)) + 1e-4f;
    const float3 H = normalize(viewDirection + lightDirection);
    const float NdotH = saturate(dot(normal, H));
    const float VdotH = saturate(dot(viewDirection, H));
    const float distribution = DistributionGGX(NdotH, roughness);
    const float geometry = GeometrySmith(NdotV, NdotL, roughness);
    const float3 fresnel = FresnelSchlick(VdotH, reflectance);
    const float3 specular = (distribution * geometry * fresnel) /
        max(4.0f * NdotV * NdotL, 1e-4f);
    const float3 diffuseWeight = (1.0f - fresnel) * (1.0f - metallic);
    result.Diffuse = diffuseWeight * baseColor / 3.14159265359f * radiance * NdotL * occlusion;
    result.Specular = specular * radiance * NdotL * occlusion;
    return result;
}

#endif
