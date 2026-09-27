// Template pixel shader for node-graph surface materials.
//
// The compiler replaces both markers below. The graph marker provides the
// material values, while the shading-model marker selects Unlit, Lit, or
// ClearCoat at shader-compilation time.

// __SHADING_MODEL__

#define MATERIAL_SHADING_MODEL_UNLIT      0
#define MATERIAL_SHADING_MODEL_LIT        1
#define MATERIAL_SHADING_MODEL_CLEAR_COAT 2

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
    float EnvIntensity;
    float EnvMaxLod;
    uint SceneColorIndex;
    uint DebugView;
    float ScreenWidth;
    float ScreenHeight;
    float4 LightDirection;
    float4 LightColor;
};

[[vk::binding(1, 0)]]
SamplerState texSampler : register(s0);

struct CompiledMaterial
{
    float4  Values[32];
    uint    TextureIndices[32];
    uint    BlendMode;
    float   AlphaCutoff;
};

[[vk::binding(2, 0)]]
StructuredBuffer<CompiledMaterial> materials;

[[vk::binding(1, 1)]]
Texture2D textures[] : register(t0);

[[vk::binding(0, 2)]]
Texture2D environmentTexture : register(t1);

[[vk::binding(1, 2)]]
Texture2D irradianceTexture : register(t2);

[[vk::binding(2, 2)]]
Texture2D prefilteredTexture : register(t3);

[[vk::binding(3, 2)]]
SamplerState environmentSampler : register(s1);

struct PushConstants
{
    float4x4 Model;
    uint MaterialIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct PSInput
{
    float4 ClipPos      : SV_Position;
    float3 Normal       : NORMAL0;
    float4 Tangent      : TANGENT0;
    float2 TexCoord     : TEXCOORD0;
    float2 TexCoord1    : TEXCOORD1;
    float3 WorldPos     : POSITION0;
    bool   FrontFace    : SV_IsFrontFace;
};

struct Surface
{
    float3  BaseColor;
    float3  Normal;
    float   Metallic;
    float   Roughness;
    float   Opacity;
    float3  Emissive;
    float   AmbientOcclusion;
    float   Specular;
    float3  SpecularColor;
    float   ClearCoat;
    float   ClearCoatRoughness;
    float3  ClearCoatBottomNormal;
};

static const uint NO_TEXTURE = 0xFFFFFFFFu;
static const uint PREFILTER_LEVEL_COUNT = 6u;
static const uint SURFACE_DEBUG_COMPOSITE = 0u;
static const uint SURFACE_DEBUG_BASE_COLOR = 1u;
static const uint SURFACE_DEBUG_DIFFUSE_IBL = 2u;
static const uint SURFACE_DEBUG_SPECULAR_IBL = 3u;
static const uint SURFACE_DEBUG_DIRECT_DIFFUSE = 4u;
static const uint SURFACE_DEBUG_DIRECT_SPECULAR = 5u;
static const uint SURFACE_DEBUG_CLEAR_COAT = 6u;

float4 SampleTex(uint index, float2 uv)
{
    return textures[index].Sample(texSampler, uv);
}

float3 SampleNormal(uint index, float2 uv)
{
    if (index == NO_TEXTURE)
        return float3(0.0f, 0.0f, 1.0f);

    const float3 packedNormal = SampleTex(index, uv).xyz;
    return normalize(packedNormal * 2.0f - 1.0f);
}

float2 DirToEquirect(float3 dir)
{
    float u = atan2(dir.z, dir.x) * 0.15915494f + 0.5f;
    float v = acos(clamp(dir.y, -1.0f, 1.0f)) * 0.31830989f;
    return float2(u, v);
}

float3 SampleIrradiance(float3 dir)
{
    return irradianceTexture.SampleLevel(
        environmentSampler,
        DirToEquirect(dir),
        0
    ).rgb * EnvIntensity;
}

float3 SampleEnvironment(float3 dir, float lod)
{
    float3 color = environmentTexture.SampleLevel(
        environmentSampler,
        DirToEquirect(dir),
        lod
    ).rgb;

    // Limit extreme HDR texels before they are scattered by a glossy BRDF.
    // This prevents isolated light sources from becoming fireflies.
    return min(color, 3.0f) * EnvIntensity;
}

float3 SamplePrefilteredLevel(float3 dir, uint level)
{
    uint width = 0;
    uint height = 0;
    prefilteredTexture.GetDimensions(width, height);

    const float levelCount = EnvMaxLod + 1.0f;
    const float blockTexelSize = levelCount / float(height);

    // Each vertical block contains one equirectangular GGX convolution. Keep
    // bilinear filtering inside the selected block so it cannot sample another
    // roughness level at the top or bottom edge.
    float2 uv = DirToEquirect(dir);

    uv.y = clamp(uv.y, 0.5f * blockTexelSize, 1.0f - 0.5f * blockTexelSize);
    uv.y = (float(level) + uv.y) / levelCount;

    return prefilteredTexture.SampleLevel(
        environmentSampler,
        uv,
        0
    ).rgb * EnvIntensity;
}

float3 SampleSpecular(float3 dir, float roughness)
{
    const float r = saturate(roughness);
    const float3 env = SampleEnvironment(dir, r * EnvMaxLod);
    if (r < 0.12f)
        return env;

    const float prefilterLevel = r * EnvMaxLod;
    const uint lowerLevel = uint(floor(prefilterLevel));
    const uint upperLevel = min(lowerLevel + 1u, uint(EnvMaxLod));
    const float blend = frac(prefilterLevel);

    const float3 lower = SamplePrefilteredLevel(dir, lowerLevel);
    const float3 upper = SamplePrefilteredLevel(dir, upperLevel);
    const float3 prefiltered = lerp(lower, upper, blend);

    return lerp(env, prefiltered, smoothstep(0.12f, 0.35f, r));
}

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

float3 FresnelSchlickRoughness(float NdotV, float3 F0, float roughness)
{
    const float3 roughnessF0 = max((1.0f - roughness).xxx, F0);
    return F0 + (roughnessF0 - F0) * pow(saturate(1.0f - NdotV), 5.0f);
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

/**
 * Kari's analytical approximation for the split-sum environment BRDF.
 *
 * This replaces a precomputed BRDF LUT while retaining the roughness and
 * view-angle response needed by image-based specular lighting.
 */
float2 EnvBRDFApprox(float roughness, float NdotV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    const float4 coefficients = roughness * c0 + c1;
    const float a004 = min(
        coefficients.x * coefficients.x,
        exp2(-9.28f * NdotV)
    ) * coefficients.x + coefficients.y;

    return float2(-1.04f, 1.04f) * a004 + coefficients.zw;
}

float4 main(PSInput input) : SV_Target0
{
    CompiledMaterial mat = materials[pc.MaterialIndex];

    float3 N = normalize(input.Normal);
    if (!input.FrontFace)
        N = -N;
    float3 V = normalize(CameraPos - input.WorldPos);

    // Defaults; the graph overrides whichever channels it drives.
    Surface surface;
    surface.BaseColor = float3(0.8f, 0.8f, 0.8f);
    surface.Normal = float3(0.0f, 0.0f, 1.0f);
    surface.Metallic = 0.0f;
    surface.Roughness = 0.5f;
    surface.Opacity = 1.0f;
    surface.Emissive = float3(0.0f, 0.0f, 0.0f);
    surface.AmbientOcclusion = 1.0f;
    surface.Specular = 1.0f;
    surface.SpecularColor = float3(1.0f, 1.0f, 1.0f);
    surface.ClearCoat = 0.0f;
    surface.ClearCoatRoughness = 0.0f;
    surface.ClearCoatBottomNormal = float3(0.0f, 0.0f, 1.0f);

    // __GRAPH_BODY__

    static const uint MATERIAL_BLEND_MASK = 1u;
    if (mat.BlendMode == MATERIAL_BLEND_MASK)
    {
        clip(surface.Opacity - mat.AlphaCutoff);
    }

#if MATERIAL_SHADING_MODEL == MATERIAL_SHADING_MODEL_UNLIT
    return float4(surface.BaseColor + surface.Emissive, surface.Opacity);
#endif

    float roughness = clamp(surface.Roughness, 0.045f, 1.0f);

    float3 dielectricF0 = min(
        0.04f.xxx * surface.SpecularColor * surface.Specular,
        1.0f.xxx
    );
    float3 F0 = lerp(dielectricF0, surface.BaseColor, surface.Metallic);

    // surface.Normal is tangent-space. Transform it into world space through
    // the orthonormal tangent basis reconstructed from the mesh vertex data.
    float3 tangentNormal = normalize(surface.Normal);
    float3 tangent = normalize(input.Tangent.xyz - N * dot(N, input.Tangent.xyz));
    float3 bitangent = cross(N, tangent) * input.Tangent.w;
    const float3x3 tangentBasis = float3x3(tangent, bitangent, N);

#if MATERIAL_SHADING_MODEL == MATERIAL_SHADING_MODEL_CLEAR_COAT
    const float3 coatNormal = normalize(mul(tangentNormal, tangentBasis));
    N = normalize(mul(normalize(surface.ClearCoatBottomNormal), tangentBasis));
#else
    N = normalize(mul(tangentNormal, tangentBasis));
#endif

    roughness = FilterSpecularRoughness(roughness, N);

    float3 R = reflect(-V, N);
    float3 L = normalize(LightDirection.xyz);
    float NdotV = saturate(dot(N, V)) + 1e-4f;
    float NdotL = saturate(dot(N, L));
    float ao = saturate(surface.AmbientOcclusion);

    // Image-based lighting uses the split-sum approximation. The irradiance map
    // provides the diffuse hemisphere integral, while the prefiltered map and
    // analytical BRDF approximate the specular hemisphere integral.
    const float3 fresnelIBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    const float3 diffuseWeightIBL = (1.0f - fresnelIBL) * (1.0f - surface.Metallic);
    const float3 diffuseIBL = SampleIrradiance(N) * surface.BaseColor;
    const float2 environmentBRDF = EnvBRDFApprox(roughness, NdotV);
    const float3 specularIBL = SampleSpecular(R, roughness) * (F0 * environmentBRDF.x + environmentBRDF.y);

    const float3 diffuseIBLContribution = diffuseWeightIBL * diffuseIBL * ao;
    const float3 specularIBLContribution = specularIBL * ao;
    float3 directDiffuseContribution = 0.0f.xxx;
    float3 directSpecularContribution = 0.0f.xxx;
    float3 clearCoatContribution = 0.0f.xxx;
    float3 color = diffuseIBLContribution + specularIBLContribution + surface.Emissive;

    // Cook-Torrance microfacet BRDF for the directional light.
    if (NdotL > 0.0f)
    {
        const float3 H = normalize(V + L);
        const float NdotH = saturate(dot(N, H));
        const float VdotH = saturate(dot(V, H));

        const float distribution = DistributionGGX(NdotH, roughness);
        const float geometry = GeometrySmith(NdotV, NdotL, roughness);
        const float3 fresnelDirect = FresnelSchlick(VdotH, F0);
        const float3 specularDirect = (distribution * geometry * fresnelDirect) /
            max(4.0f * NdotV * NdotL, 1e-4f);

        const float3 diffuseWeightDirect = (1.0f - fresnelDirect) * (1.0f - surface.Metallic);
        const float3 radiance = LightColor.rgb * LightColor.a;

        directDiffuseContribution = diffuseWeightDirect * surface.BaseColor /
            3.14159265359f * radiance * NdotL * ao;
        directSpecularContribution = specularDirect * radiance * NdotL * ao;
        color += directDiffuseContribution + directSpecularContribution;
    }

#if MATERIAL_SHADING_MODEL == MATERIAL_SHADING_MODEL_CLEAR_COAT
    const float clearCoat = saturate(surface.ClearCoat);
    if (clearCoat > 0.0f)
    {
        float coatRoughness = saturate(surface.ClearCoatRoughness);
        const float coatNdotV = saturate(dot(coatNormal, V)) + 1e-4f;
        const float coatNdotL = saturate(dot(coatNormal, L));

        const float3 coatF0 = 0.04f.xxx;
        const float coatFresnel = clearCoat * FresnelSchlick(coatNdotV, coatF0).x;
        const float2 coatEnvBRDF = EnvBRDFApprox(coatRoughness, coatNdotV);
        const float3 coatReflection = SampleSpecular(reflect(-V, coatNormal), coatRoughness) *
            (coatF0 * coatEnvBRDF.x + coatEnvBRDF.y);

        clearCoatContribution = coatReflection * clearCoat;
        color = color * (1.0f - coatFresnel) + clearCoatContribution;

        if (coatNdotL > 0.0f)
        {
            const float3 coatHalfVector = normalize(V + L);
            const float coatNdotH = saturate(dot(coatNormal, coatHalfVector));
            const float coatVdotH = saturate(dot(V, coatHalfVector));

            const float coatDistribution = DistributionGGX(coatNdotH, coatRoughness);
            const float coatGeometry = GeometrySmith(coatNdotV, coatNdotL, coatRoughness);
            const float coatFresnelDirect = clearCoat * FresnelSchlick(coatVdotH, coatF0).x;
            const float coatSpecular = coatDistribution * coatGeometry * coatFresnelDirect /
                max(4.0f * coatNdotV * coatNdotL, 1e-4f);

            const float3 coatDirectContribution = coatSpecular * LightColor.rgb *
                LightColor.a * coatNdotL;
            clearCoatContribution += coatDirectContribution;
            color += coatDirectContribution;
        }
    }
#endif

    if (DebugView == SURFACE_DEBUG_BASE_COLOR)
        color = surface.BaseColor;
    else if (DebugView == SURFACE_DEBUG_DIFFUSE_IBL)
        color = diffuseIBLContribution;
    else if (DebugView == SURFACE_DEBUG_SPECULAR_IBL)
        color = specularIBLContribution;
    else if (DebugView == SURFACE_DEBUG_DIRECT_DIFFUSE)
        color = directDiffuseContribution;
    else if (DebugView == SURFACE_DEBUG_DIRECT_SPECULAR)
        color = directSpecularContribution;
    else if (DebugView == SURFACE_DEBUG_CLEAR_COAT)
        color = clearCoatContribution;

    return float4(color, surface.Opacity);
}
