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
    float4 TangentRibbonId : TEXCOORD2;
    float4 Color           : COLOR;
    float4 Metadata        : TEXCOORD3;
};

struct VSOutput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;
    float2 TexCoord      : TEXCOORD0;
};

// TODO: TEMP approximation, replace by a better solution!
float MaxAxisScale(float4x4 transform)
{
    return max(
        length(transform[0].xyz),
        max(length(transform[1].xyz),length(transform[2].xyz))
    );
}

VSOutput main(VSInput input, uint vertexId : SV_VertexID)
{
    VSOutput output;

    // Generate quad positions using bit manipulation
    float2 normalizedPos = CalculateQuadPosition(vertexId % 6);

    float size = input.PositionSize.w *
        max(input.Transform.y, 0.0f) *
        MaxAxisScale(pc.WorldTransform);

    float3 worldPosition = mul(
        pc.WorldTransform,
        float4(input.PositionSize.xyz, 1.0f)
    ).xyz;

    float3 viewPos = mul(View, float4(worldPosition, 1.0f)).xyz;
    viewPos.xy += (normalizedPos - 0.5f) * size; // Center around origin

    output.ClipPos = mul(Proj, float4(viewPos, 1.0f));
    output.Color = input.Color;
    output.TexCoord = normalizedPos;

    return output;
}