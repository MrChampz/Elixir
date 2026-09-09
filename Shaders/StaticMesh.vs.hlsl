[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 ViewProjection;
};

struct PushConstants
{
    float4 Color;
};

[[vk::push_constant]]
PushConstants pc;

struct VSInput
{
    float3 Position : POSITION0;
    float3 Normal : NORMAL0;
    float4 Tangent : TANGENT0;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(ViewProjection, float4(input.Position, 1.0f));
    output.Color = pc.Color;
    return output;
}
