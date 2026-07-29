// Template for EMaterialUsage::ParticleSprite. The generated graph body writes
// Surface fields using particle color, UV, time, material values and textures.

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
    uint SpriteIndex;
    uint MaterialIndex;
};

[[vk::push_constant]]
MaterialPushConstants pc;

struct PSInput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;
    float2 TexCoord      : TEXCOORD0;
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
    CompiledMaterial mat = materials[pc.MaterialIndex];

    Surface surface;
    surface.BaseColor = float3(1.0f, 1.0f, 1.0f);
    surface.Normal = float3(0.0f, 0.0f, 1.0f);
    surface.Metallic = 0.0f;
    surface.Roughness = 0.5f;
    surface.Emissive = float3(0.0f, 0.0f, 0.0f);

    // __GRAPH_BODY__

    const float4 sprite = sprites[pc.SpriteIndex].Sample(spriteSampler, input.TexCoord);
    const float3 color = input.Color.rgb * sprite.rgb * surface.BaseColor + surface.Emissive;
    const float alpha = input.Color.a * sprite.a;

    return float4(color, alpha);
}