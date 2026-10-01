// Shader variant for EMaterialUsage::Particle. The generated graph body writes
// Surface fields using UV, time, material values and textures.

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
    float4x4 WorldTransform;
    uint MaterialIndex;
};

[[vk::push_constant]]
MaterialPushConstants pc;

struct PSInput
{
    float4 ClipPos       : SV_POSITION;
    float2 TexCoord      : TEXCOORD0;
    float2 TexCoord1     : TEXCOORD1;
    float3 ViewPos       : POSITION0;
    float4 Color : COLOR;
};

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

    float3 V = normalize(-input.ViewPos);
    float3 N = normalize(cross(ddy(input.ViewPos), ddx(input.ViewPos)));
    N = faceforward(N, -V, N);

    // __GRAPH_BODY__

    static const uint MATERIAL_BLEND_MASK = 1u;
    if (mat.BlendMode == MATERIAL_BLEND_MASK)
    {
        clip(surface.Opacity - mat.AlphaCutoff);
    }

    return float4(surface.BaseColor + surface.Emissive, surface.Opacity);
}
