// Template for EMaterialUsage::ParticleMesh. The generated graph body writes
// Surface fields using mesh colors, planar UVs, time, material values and textures.

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
    float4 Values[32];
    uint TextureIndices[32];
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
    float4 Color    : COLOR0;
    float2 TexCoord : TEXCOORD0;
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

float4 SampleTex(uint index, float2 uv)
{
    return sprites[index].Sample(spriteSampler, uv);
}

float4 main(PSInput input) : SV_Target0
{
    CompiledMaterial mat = materials[pc.MaterialIndex];

    Surface surface;
    surface.BaseColor = float3(1.0f, 1.0f, 1.0f);
    surface.Normal = float3(0.0f, 0.0f, 1.0f);
    surface.Metallic = 0.0f;
    surface.Roughness = 0.5f;
    surface.Opacity = 1.0f;
    surface.Emissive = float3(0.0f, 0.0f, 0.0f);

    // __GRAPH_BODY__

    const float3 normal = normalize(input.Normal);
    const float3 lightDirection = normalize(LIGHT_DIRECTION);
    const float3 viewDirection = normalize(CameraPos - input.WorldPos);

    const float diffuse = max(dot(normal, lightDirection), 0.0f);
    const float rim = pow(1.0f - max(dot(normal, viewDirection), 0.0f), 2.8f);
    const float3 litColor = (surface.BaseColor * (0.24f + diffuse * 0.92f)) +
        surface.Emissive +
        (RIM_COLOR * rim * 0.35f);

    return float4(litColor, surface.Opacity);
}