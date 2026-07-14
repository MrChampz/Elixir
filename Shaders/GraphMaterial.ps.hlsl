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
    float3 Normal; // tangent-space perturbation (unused by the simple template)
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

    float3 diffuse = SampleIrradiance(N) * surface.BaseColor * (1.0f - surface.Metallic);
    float3 R = reflect(-V, N);
    float3 fresnel = F0 + (max((1.0f - roughness).xxx, F0) - F0) * pow(saturate(1.0f - NdotV), 5.0f);
    float3 specular = SampleEnv(R, roughness) * fresnel;

    float3 color = diffuse + specular + surface.Emissive;

    // Keep the 'materials' storage buffer binding alive through DXC dead-code
    // elimination even when the graph reads no material parameter. The scale is
    // data-dependent (a runtime buffer value) so the optimiser can't fold it away,
    // yet its contribution is imperceptible.
    color += mat.BaseColorFactor.rgb * 1e-8f;

    // Directional light: Lambert diffuse + a simple spec.
    float3 L = normalize(LightDirection.xyz);
    float NdotL = saturate(dot(N, L));
    float3 H = normalize(V + L);
    float spec = pow(saturate(dot(N, H)), max(2.0f, (1.0f - roughness) * 128.0f));
    color += (surface.BaseColor * (1.0f - surface.Metallic) + F0 * spec) * LightColor.rgb * LightColor.w * NdotL;

    return float4(ACESFilm(color), 1.0f);
}
