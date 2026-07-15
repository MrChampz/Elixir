// Template pixel shader for node-graph materials. The graph codegen fills the
// surface struct at the __GRAPH_BODY__ marker; the rest is fixed shading shared
// by every graph material (IBL diffuse + specular, ACES tonemap).

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
    uint EnvIndex;
    uint IrradianceIndex;
    float EnvIntensity;
    float EnvMaxLod;
    uint PrefIndex;
    uint SceneColorIndex;
    float ScreenWidth;
    float ScreenHeight;
    float4 LightDirection;
    float4 LightColor;
};

[[vk::binding(1, 0)]]
SamplerState texSampler : register(s0);

struct GPUMaterial
{
    float4 BaseColorFactor;
    float4 EmissiveMetallic;     // xyz = emissive, w = metallic
    float4 RoughOccNormalCutoff; // x = roughness, y = occlusion, z = normalScale, w = alphaCutoff
    uint4 TexIndex0;
    uint4 TexIndex1;
    float4 BaseColorTransform;
    float4 MetallicRoughnessTransform;
    float4 NormalTransform;
    float4 EmissiveTransform;
    float4 OcclusionTransform;
    float4 Clearcoat;
    float4 Specular;
};

[[vk::binding(2, 0)]]
StructuredBuffer<GPUMaterial> materials;

// User-exposed graph parameters (ParamScalar/ParamColor nodes). The app updates
// these live without recompiling the shader.
[[vk::binding(3, 0)]]
cbuffer cbGraphParams : register(b1)
{
    float4 GraphParams[8];
};

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

// The surface the graph drives; fixed shading below consumes it.
struct SSurface
{
    float3 BaseColor;
    float Metallic;
    float Roughness;
    float3 Emissive;
    float3 Normal;  // tangent-space perturbation
    float Opacity;  // alpha (Masked clip / Translucent blend)
};

static const uint NO_TEXTURE = 0xffffffffu;

float3 SampleTex(uint index, float2 uv)
{
    return textures[index].Sample(texSampler, uv).rgb;
}

// --- Procedural pattern helpers (driven by graph nodes) ---
float Checker(float2 uv, float scale)
{
    float2 c = floor(uv * scale);
    return frac((c.x + c.y) * 0.5f) < 0.25f ? 0.0f : 1.0f;
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 uv, float scale)
{
    uv *= scale;
    float2 i = floor(uv);
    float2 f = frac(uv);
    f = f * f * (3.0f - 2.0f * f); // smoothstep interpolation
    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// 3D variants (driven by a Position node) so procedural patterns work on meshes
// with degenerate UVs -- e.g. solid-paint car body panels.
float Checker3(float3 p, float scale)
{
    float3 c = floor(p * scale);
    return frac((c.x + c.y + c.z) * 0.5f) < 0.25f ? 0.0f : 1.0f;
}

float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.zyx + 31.32f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3(float3 pos, float scale)
{
    pos *= scale;
    float3 i = floor(pos);
    float3 f = frac(pos);
    f = f * f * (3.0f - 2.0f * f);
    float x00 = lerp(Hash31(i + float3(0, 0, 0)), Hash31(i + float3(1, 0, 0)), f.x);
    float x10 = lerp(Hash31(i + float3(0, 1, 0)), Hash31(i + float3(1, 1, 0)), f.x);
    float x01 = lerp(Hash31(i + float3(0, 0, 1)), Hash31(i + float3(1, 0, 1)), f.x);
    float x11 = lerp(Hash31(i + float3(0, 1, 1)), Hash31(i + float3(1, 1, 1)), f.x);
    return lerp(lerp(x00, x10, f.y), lerp(x01, x11, f.y), f.z);
}

float2 DirToEquirect(float3 d)
{
    float u = atan2(d.z, d.x) * 0.15915494f + 0.5f;
    float v = acos(clamp(d.y, -1.0f, 1.0f)) * 0.31830989f;
    return float2(u, v);
}

float3 SampleIrradiance(float3 dir)
{
    if (IrradianceIndex == NO_TEXTURE)
        return float3(0.1f, 0.12f, 0.15f);
    return textures[IrradianceIndex].SampleLevel(texSampler, DirToEquirect(dir), 0).rgb * EnvIntensity;
}

float3 SampleEnv(float3 dir, float roughness)
{
    if (EnvIndex == NO_TEXTURE)
        return float3(0.1f, 0.12f, 0.15f);
    return textures[EnvIndex].SampleLevel(texSampler, DirToEquirect(dir), roughness * EnvMaxLod).rgb * EnvIntensity;
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Shading models (the graph injects `const uint SHADING_MODEL = N;`). Compile-time
// constant, so DXC strips the branches for the models a given material doesn't use.
#define SM_DEFAULT    0u
#define SM_UNLIT      1u
#define SM_SUBSURFACE 2u
#define SM_CLEARCOAT  3u
#define SM_CLOTH      4u

// The material compiler replaces this marker with `#define SHADING_MODEL <n>`.
// __SHADING_MODEL__
#ifndef SHADING_MODEL
#define SHADING_MODEL SM_DEFAULT
#endif

float4 main(PSInput input) : SV_Target0
{
    GPUMaterial mat = materials[pc.MaterialIndex];

    float3 N = normalize(input.Normal);
    if (!input.FrontFace)
        N = -N;
    float3 V = normalize(CameraPos - input.WorldPos);

    // Defaults; the graph overrides whichever channels it drives.
    SSurface surface;
    surface.BaseColor = float3(0.8f, 0.8f, 0.8f);
    surface.Metallic = 0.0f;
    surface.Roughness = 0.5f;
    surface.Emissive = float3(0.0f, 0.0f, 0.0f);
    surface.Normal = float3(0.0f, 0.0f, 1.0f);
    surface.Opacity = 1.0f;

    // __GRAPH_BODY__

    // Apply the graph's tangent-space normal (default (0,0,1) leaves the geometric
    // normal unchanged). Guarded so meshes without tangents stay stable.
    if (dot(input.Tangent.xyz, input.Tangent.xyz) > 1e-8f)
    {
        float3 T = normalize(input.Tangent.xyz - N * dot(N, input.Tangent.xyz));
        float3 B = cross(N, T) * input.Tangent.w;
        N = normalize(mul(surface.Normal, float3x3(T, B, N)));
    }

    float roughness = clamp(surface.Roughness, 0.045f, 1.0f);
    float3 F0 = lerp(0.04f.xxx, surface.BaseColor, surface.Metallic);
    float NdotV = saturate(dot(N, V)) + 1e-4f;
    float3 L = normalize(LightDirection.xyz);
    float NdotL = saturate(dot(N, L));
    float3 H = normalize(V + L);
    float3 lightRad = LightColor.rgb * LightColor.w;

    float3 color;
    if (SHADING_MODEL == SM_UNLIT)
    {
        // No lighting: just the (tonemapped) base colour plus emissive.
        color = surface.BaseColor + surface.Emissive;
    }
    else
    {
        // Default lit: IBL diffuse + specular, plus a directional key light.
        float3 diffuse = SampleIrradiance(N) * surface.BaseColor * (1.0f - surface.Metallic);
        float3 R = reflect(-V, N);
        float3 fresnel = F0 + (max((1.0f - roughness).xxx, F0) - F0) * pow(saturate(1.0f - NdotV), 5.0f);
        float3 specular = SampleEnv(R, roughness) * fresnel;
        color = diffuse + specular + surface.Emissive;

        float spec = pow(saturate(dot(N, H)), max(2.0f, (1.0f - roughness) * 128.0f));
        color += (surface.BaseColor * (1.0f - surface.Metallic) + F0 * spec) * lightRad * NdotL;

        if (SHADING_MODEL == SM_SUBSURFACE)
        {
            // Wrapped diffuse + back-face translucency, tinted by the base colour.
            float wrap = saturate((dot(N, L) + 0.5f) / 1.5f);
            float back = pow(saturate(dot(V, -L)), 4.0f) * 0.6f;
            color += (wrap * 0.35f + back) * surface.BaseColor * lightRad;
            color += SampleIrradiance(-N) * surface.BaseColor * 0.25f;
        }
        else if (SHADING_MODEL == SM_CLEARCOAT)
        {
            // A second, sharp clear-coat specular layer on top.
            float ccF = 0.04f + 0.96f * pow(saturate(1.0f - NdotV), 5.0f);
            color += SampleEnv(reflect(-V, N), 0.06f) * ccF;
            color += pow(saturate(dot(N, H)), 256.0f) * ccF * lightRad * NdotL;
        }
        else if (SHADING_MODEL == SM_CLOTH)
        {
            // Fuzzy sheen that brightens at grazing angles.
            float sheen = pow(1.0f - NdotV, 3.0f);
            color += sheen * surface.BaseColor * 0.5f * (SampleIrradiance(N) + lightRad * NdotL);
        }
    }

    // Keep the 'materials' and 'cbGraphParams' bindings alive through DXC dead-code
    // elimination even when the graph reads neither (imperceptible, data-dependent).
    color += mat.BaseColorFactor.rgb * 1e-8f + GraphParams[0].rgb * 1e-8f;

    return float4(ACESFilm(color), surface.Opacity);
}
