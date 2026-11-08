// Shader variant for EMaterialUsage::Particle. The generated graph body writes
// Surface fields using planar UVs, time, material values and textures.

#include "../Material/NormalSampling.hlsl"
#include "../Material/NormalTransforms.hlsl"

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4    View;
    float4x4    Proj;
    float4x4    ViewProj;
    float3      CameraPos;
    float       Time;
};

[[vk::binding(1, 0)]]
SamplerState spriteSampler : register(s0);

[[vk::binding(1, 1)]]
Texture2D sprites[] : register(t0);

struct CompiledMaterial
{
    float4  Values[32];
    uint    TextureIndices[32];
    uint    BlendMode;
    float   AlphaCutoff;
};

[[vk::binding(2, 0)]]
StructuredBuffer<CompiledMaterial> materials;

struct MaterialPushConstants
{
    float4x4    WorldTransform;
    uint        MaterialIndex;
};

[[vk::push_constant]]
MaterialPushConstants pc;

struct PSInput
{
    float4 ClipPos  : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 Normal   : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
    float4 Color : COLOR;
};

static const float3 LIGHT_DIRECTION = float3(-0.45, 0.8, 0.55);
static const float3 RIM_COLOR = float3(0.35, 0.42, 0.52);

struct Surface
{
    float3  BaseColor;
    float3  Normal;
    float   Metallic;
    float   Roughness;
    float   Opacity;
    float3  Emissive;
};

[[vk::binding(8, 0)]]
SamplerState linearClamp;

[[vk::binding(9, 0)]]
SamplerState pointWrap;

[[vk::binding(10, 0)]]
SamplerState pointClamp;

float4 main(PSInput input) : SV_Target0
{
    CompiledMaterial mat = materials[pc.MaterialIndex];

    const float3 geometricNormal = normalize(input.Normal);
    float3 N = geometricNormal;
    const float3 V = normalize(CameraPos - input.WorldPos);

    Surface surface;
    surface.BaseColor = float3(1.0f, 1.0f, 1.0f);
    surface.Metallic = 0.0f;
    surface.Roughness = 0.5f;
    surface.Opacity = 1.0f;
    surface.Emissive = float3(0.0f, 0.0f, 0.0f);

#if MATERIAL_TANGENT_SPACE_NORMAL
    surface.Normal = float3(0.0f, 0.0f, 1.0f);
#else
    surface.Normal = geometricNormal;
#endif

    // __GRAPH_BODY__

#if MATERIAL_TANGENT_SPACE_NORMAL
    N = TangentNormalToWorld(surface.Normal, geometricNormal, input.WorldPos, input.TexCoord);
#else
    N = normalize(surface.Normal);
#endif

    static const uint MATERIAL_BLEND_MASK = 1u;
    if (mat.BlendMode == MATERIAL_BLEND_MASK)
    {
        clip(surface.Opacity - mat.AlphaCutoff);
    }

    const float3 lightDirection = normalize(LIGHT_DIRECTION);

    const float diffuse = max(dot(N, lightDirection), 0.0f);
    const float rim = pow(1.0f - max(dot(N, V), 0.0f), 2.8f);
    const float3 litColor = (surface.BaseColor * (0.24f + diffuse * 0.92f)) +
        surface.Emissive + (RIM_COLOR * rim * 0.35f);

    return float4(litColor, surface.Opacity);
}
