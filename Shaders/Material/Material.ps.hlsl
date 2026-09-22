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
    float EnvIntensity;
    float EnvMaxLod;
    uint SceneColorIndex;
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
};

static const uint NO_TEXTURE = 0xFFFFFFFFu;

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

float3 SampleEnv(float3 dir, float roughness)
{
    const float prefilterLevel = saturate(roughness) * EnvMaxLod;
    const float prefilterLevels = EnvMaxLod + 1.0f;

    // The prefiltered texture is an equirectangular atlas. Each vertical block
    // stores the GGX convolution for one roughness level.
    float2 uv = DirToEquirect(dir);
    uv.y = (uv.y + prefilterLevel) / prefilterLevels;

    return prefilteredTexture.SampleLevel(
        environmentSampler,
        uv,
        0
    ).rgb * EnvIntensity;
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
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

    // __GRAPH_BODY__

    static const uint MATERIAL_BLEND_MASK = 1u;
    if (mat.BlendMode == MATERIAL_BLEND_MASK)
    {
        clip(surface.Opacity - mat.AlphaCutoff);
    }

    float roughness = clamp(surface.Roughness, 0.045f, 1.0f);
    float3 F0 = lerp(0.04f.xxx, surface.BaseColor, surface.Metallic);

    // surface.Normal is tangent-space. Transform it into world space through
    // the orthonormal tangent basis reconstructed from the mesh vertex data.
    float3 tangentNormal = normalize(surface.Normal);
    float3 tangent = normalize(input.Tangent.xyz - N * dot(N, input.Tangent.xyz));
    float3 bitangent = cross(N, tangent) * input.Tangent.w;
    N = normalize(mul(tangentNormal, float3x3(tangent, bitangent, N)));

    float NdotV = saturate(dot(N, V)) + 1e-4f;

    float ao = saturate(surface.AmbientOcclusion);

    float3 diffuse = SampleIrradiance(N) * surface.BaseColor * (1.0f - surface.Metallic) * ao;
    float3 R = reflect(-V, N);

    float3 fresnel = F0 + (max((1.0f - roughness).xxx, F0) - F0) * pow(saturate(1.0f - NdotV), 5.0f);
    float3 specular = SampleEnv(R, roughness) * fresnel;

    float3 color = diffuse + specular + surface.Emissive;

    // Direcional light: Lambert diffuse + a simple spec.
    float3 L = normalize(LightDirection.xyz);
    float NdotL = saturate(dot(N, L));
    float3 H = normalize(V + L);
    float spec = pow(saturate(dot(N, H)), max(2.0f, (1.0f - roughness) * 128.0f));
    color += (surface.BaseColor * (1.0f - surface.Metallic) + F0 * spec) * LightColor.rgb * LightColor.w * NdotL * ao;

    // Tone mapping
    color = ACESFilm(color);

    return float4(color, surface.Opacity);
}