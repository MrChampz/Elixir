// Shader variant for EMaterialUsage::Particle. It shares sprite geometry while
// exposing only the varyings used by material particle fragment shaders.

#include "../Quad.hlsl"

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct PushConstants
{
    float4x4 WorldTransform;
    uint MaterialIndex;
    uint SpriteIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct VSInput
{
    float4 PositionSize    : POSITION;
    float4 VelocityAge     : TEXCOORD0;
    float4 Transform       : TEXCOORD1;
    float4 TangentGroupId  : TEXCOORD2;
    float4 Color           : COLOR;
    float4 Metadata        : TEXCOORD3;
};

struct VSOutput
{
    float4 ClipPos      : SV_POSITION;
    float2 TexCoord     : TEXCOORD0;
    float2 TexCoord1    : TEXCOORD1;
    float3 ViewPos      : POSITION0;
    float3 WorldPos     : POSITION1;
    float4 Color        : COLOR;
    float2 ParticleTime : TEXCOORD2;
};

float MaxAxisScale(float4x4 transform)
{
    return max(
        length(transform[0].xyz),
        max(length(transform[1].xyz), length(transform[2].xyz))
    );
}

VSOutput main(VSInput input, uint vertexId : SV_VertexID)
{
    VSOutput output;
    output.ParticleTime = float2(input.VelocityAge.w, input.Metadata.z);

    const float2 normalizedPos = CalculateQuadPosition(vertexId % 6);
    const float size = input.PositionSize.w *
        max(input.Transform.y, 0.0f) *
        MaxAxisScale(pc.WorldTransform);

    const float3 worldPosition = mul(
        pc.WorldTransform,
        float4(input.PositionSize.xyz, 1.0f)
    ).xyz;

    float sine, cosine;
    sincos(input.Transform.x, sine, cosine);

    float2 offset = (normalizedPos - 0.5f) * size;
    offset = float2(
        cosine * offset.x - sine * offset.y,
        sine * offset.x + cosine * offset.y
    );

    const float3 viewPos = mul(View, float4(worldPosition, 1.0f)).xyz + float3(offset, 0.0f);

    output.ClipPos = mul(Proj, float4(viewPos, 1.0f));
    // Texture UVs start at the top-left; billboard positions increase upward.
    output.TexCoord = float2(normalizedPos.x, 1.0f - normalizedPos.y);
    output.TexCoord1 = output.TexCoord;
    output.ViewPos = viewPos;

    // View has an orthonormal rotation; transpose it to reconstruct the billboard in world space.
    output.WorldPos = CameraPos + mul(transpose((float3x3)View), viewPos);

    output.Color = input.Color;

    return output;
}
