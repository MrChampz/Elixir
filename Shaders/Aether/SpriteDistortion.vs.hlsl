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
    float4 ClipPos : SV_POSITION;
    float4 Color   : COLOR;
    float2 QuadUV  : TEXCOORD0;
};

VSOutput main(VSInput input, uint vertexId : SV_VertexID)
{
    VSOutput output;

    float2 normalizedPos = CalculateQuadPosition(vertexId % 6);

    float size = input.PositionSize.w * max(input.Transform.y, 0.0f);

    float3 viewPos = mul(View, float4(input.PositionSize.xyz, 1.0f)).xyz;
    viewPos.xy += (normalizedPos - 0.5f) * size;

    output.ClipPos = mul(Proj, float4(viewPos, 1.0f));
    output.Color = input.Color;
    output.QuadUV = float2(normalizedPos.x, 1.0f - normalizedPos.y);

    return output;
}
