// glTF metallic-roughness PBR base pass. Cook-Torrance GGX BRDF with a single
// directional light plus a flat ambient term. Normal mapping uses a
// derivative-based cotangent frame (no TANGENT attribute required). IBL comes
// later; for now metals reflect the constant ambient.

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
    uint EnvIndex;
    uint IrradianceIndex;
    float EnvIntensity;
    float _Padding2;
};

[[vk::binding(1, 0)]]
SamplerState texSampler : register(s0);

struct GPUMaterial
{
    float4 BaseColorFactor;
    float4 EmissiveMetallic;     // xyz = emissive, w = metallic
    float4 RoughOccNormalCutoff; // x = roughness, y = occlusion, z = normalScale, w = alphaCutoff
    uint4 TexIndex0;             // baseColor, metallicRoughness, normal, emissive
    uint4 TexIndex1;             // occlusion, alphaMode, unused...
    float4 BaseColorTransform;   // KHR_texture_transform: uv scale.xy, offset.zw
    float4 MetallicRoughnessTransform;
    float4 NormalTransform;
    float4 EmissiveTransform;
    float4 OcclusionTransform;
};

[[vk::binding(2, 0)]]
StructuredBuffer<GPUMaterial> materials;

[[vk::binding(1, 1)]]
Texture2D textures[] : register(t0);

struct ModelPushConstants
{
    float4x4 Model;
    uint MaterialIndex;
};
[[vk::push_constant]]
ModelPushConstants pc;

struct PSInput
{
    float4 ClipPos   : SV_POSITION;
    float3 Normal    : NORMAL0;
    float4 Tangent   : TANGENT0;
    float2 TexCoord  : TEXCOORD0;
    float3 WorldPos  : POSITION0;
    bool FrontFace   : SV_IsFrontFace;
};

static const float PI = 3.14159265359f;
static const uint NO_TEXTURE = 0xffffffffu;

float2 XformUV(float2 uv, float4 t)
{
    return uv * t.xy + t.zw;
}

float3 SampleTex(uint index, float2 uv)
{
    return textures[index].Sample(texSampler, uv).rgb;
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-7f);
}

float GeometrySchlickGGX(float NdotX, float k)
{
    return NdotX / (NdotX * (1.0f - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return GeometrySchlickGGX(NdotV, k) * GeometrySchlickGGX(NdotL, k);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Equirectangular direction -> UV (matches the CPU irradiance bake).
float2 DirToEquirect(float3 d)
{
    float u = atan2(d.z, d.x) * 0.15915494f + 0.5f;
    float v = acos(clamp(d.y, -1.0f, 1.0f)) * 0.31830989f;
    return float2(u, v);
}

float3 SampleEnv(float3 dir)
{
    if (EnvIndex == NO_TEXTURE)
        return float3(0.1f, 0.12f, 0.15f);
    return textures[EnvIndex].SampleLevel(texSampler, DirToEquirect(dir), 0).rgb * EnvIntensity;
}

float3 SampleIrradiance(float3 dir)
{
    if (IrradianceIndex == NO_TEXTURE)
        return float3(0.1f, 0.12f, 0.15f);
    return textures[IrradianceIndex].SampleLevel(texSampler, DirToEquirect(dir), 0).rgb * EnvIntensity;
}

// Cheap runtime specular prefilter: average a small disk of directions around the
// reflection, widening with roughness (a stand-in for a prefiltered mip chain).
float3 PrefilterEnv(float3 R, float roughness)
{
    float3 base = SampleEnv(R);
    if (roughness < 0.12f)
        return base; // near-mirror: one tap

    float3 up = abs(R.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 T = normalize(cross(up, R));
    float3 B = cross(R, T);
    float radius = roughness * 0.5f;

    float3 sum = base;
    const int SEGS = 6;
    [unroll] for (int s = 0; s < SEGS; ++s)
    {
        float ang = ((float)s / SEGS) * 6.2831853f;
        float3 dir = normalize(R + (T * cos(ang) + B * sin(ang)) * radius);
        sum += SampleEnv(dir);
    }
    return sum / (SEGS + 1);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 Fr = max((1.0f - roughness).xxx, F0);
    return F0 + (Fr - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Karis' analytic environment-BRDF (split-sum) approximation -> no BRDF LUT.
float2 EnvBRDFApprox(float roughness, float NdotV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NdotV)) * r.x + r.y;
    return float2(-1.04f, 1.04f) * a004 + r.zw;
}

float4 main(PSInput input) : SV_Target0
{
    GPUMaterial mat = materials[pc.MaterialIndex];

    float4 baseColor = mat.BaseColorFactor;
    if (mat.TexIndex0.x != NO_TEXTURE)
        baseColor *= textures[mat.TexIndex0.x].Sample(texSampler, XformUV(input.TexCoord, mat.BaseColorTransform));

    // Alpha mask: discard fully-cut fragments (alphaMode == 1).
    uint alphaMode = mat.TexIndex1.y;
    if (alphaMode == 1u && baseColor.a < mat.RoughOccNormalCutoff.w)
        discard;

    float3 albedo = baseColor.rgb;
    float metallic = mat.EmissiveMetallic.w;
    float roughness = mat.RoughOccNormalCutoff.x;
    if (mat.TexIndex0.y != NO_TEXTURE)
    {
        float3 mr = SampleTex(mat.TexIndex0.y, XformUV(input.TexCoord, mat.MetallicRoughnessTransform));
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.045f, 1.0f);

    float ao = 1.0f;
    if (mat.TexIndex1.x != NO_TEXTURE)
        ao = lerp(1.0f, SampleTex(mat.TexIndex1.x, XformUV(input.TexCoord, mat.OcclusionTransform)).r, mat.RoughOccNormalCutoff.y);

    float3 emissive = mat.EmissiveMetallic.xyz;
    if (mat.TexIndex0.w != NO_TEXTURE)
        emissive *= SampleTex(mat.TexIndex0.w, XformUV(input.TexCoord, mat.EmissiveTransform));

    float3 Ngeom = normalize(input.Normal);
    if (!input.FrontFace)
        Ngeom = -Ngeom; // double-sided: face the geometric normal toward the viewer
    float3 N = Ngeom;
    float3 V = normalize(CameraPos - input.WorldPos);
    if (mat.TexIndex0.z != NO_TEXTURE)
    {
        float3 tn = SampleTex(mat.TexIndex0.z, XformUV(input.TexCoord, mat.NormalTransform)) * 2.0f - 1.0f;
        tn.xy *= mat.RoughOccNormalCutoff.z;
        // TBN from the mesh's vertex tangent (Gram-Schmidt orthonormalise).
        float3 T = normalize(input.Tangent.xyz - Ngeom * dot(Ngeom, input.Tangent.xyz));
        float3 B = cross(Ngeom, T) * input.Tangent.w;
        float3x3 TBN = float3x3(T, B, Ngeom);
        N = normalize(mul(tn, TBN));
    }

    float NdotV = saturate(dot(N, V)) + 1e-4f;
    float3 F0 = lerp(0.04f.xxx, albedo, metallic);
    // The reflection uses a roughness-faded normal: rougher surfaces reflect with
    // a smoother normal, so the paint's fine normal map stops speckling the
    // environment reflection (while smooth trim keeps full detail).
    float3 Nspec = normalize(lerp(N, Ngeom, roughness));
    float3 R = reflect(-V, Nspec);

    // Pure image-based lighting from the HDR environment: diffuse from the baked
    // irradiance map, specular from the roughness-prefiltered reflection weighted
    // by the analytic environment BRDF. The HDR's own sun provides the highlight.
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kd = (1.0f - F) * (1.0f - metallic);

    float3 irradiance = SampleIrradiance(N);
    float3 diffuse = irradiance * albedo;

    float3 prefiltered = PrefilterEnv(R, roughness);
    float2 envBRDF = EnvBRDFApprox(roughness, NdotV);
    float3 specular = prefiltered * (F0 * envBRDF.x + envBRDF.y);

    float3 color = (kd * diffuse + specular) * ao + emissive;
    color = ACESFilm(color);

    return float4(color, baseColor.a);
}
