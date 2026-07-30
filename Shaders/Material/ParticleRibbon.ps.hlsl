// Template for EMaterialUsage::ParticleRibbon. The generated graph body writes
// Surface fields using ribbon vertex color, UV, material values and textures.

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
    float4x4 WorldTransform;
    uint EmitterIndex;
    uint ParticleBaseOffset;
    uint MaterialIndex;
};

[[vk::push_constant]]
MaterialPushConstants pc;

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    nointerpolation float Valid : TEXCOORD1;
};

struct Surface
{
    float3  BaseColor;
    float3  Normal;
    float   Metallic;
    float   Roughness;
    float3  Emissive;
};

float3 SampleTex(uint index, float2 uv)
{
    return sprites[index].Sample(spriteSampler, uv).rgb;
}

float4 main(PSInput input) : SV_Target0
{
    clip(input.Valid - 0.5);

    CompiledMaterial mat = materials[pc.MaterialIndex];

    Surface surface;
    surface.BaseColor = float3(1.0f, 1.0f, 1.0f);
    surface.Normal = float3(0.0f, 0.0f, 1.0f);
    surface.Metallic = 0.0f;
    surface.Roughness = 0.5f;
    surface.Emissive = float3(0.0f, 0.0f, 0.0f);

    // __GRAPH_BODY__

    const float centeredAcrossRibbon = abs((input.UV.x * 2.0f) - 1.0f);
    const float edgeFade = 1.0f - smoothstep(0.72f, 1.0f, centeredAcrossRibbon);
    const float coreGlow = 1.0f - smoothstep(0.0f, 0.52f, centeredAcrossRibbon);
    const float3 color = (input.Color.rgb * surface.BaseColor) +
        surface.Emissive + (coreGlow * 0.22f);

    return float4(color, input.Color.a * edgeFade);
}