[[vk::binding(0, 0)]]
cbuffer cbPerFrame : register(b0)
{
    float4x4 Proj;          // Projection matrix for orthographic projection
}

struct VSInput
{
    float2 Pos    : POSITION;
    float4 Color  : COLOR;
};

struct VSOutput
{
    float4 ClipPos   : SV_Position;
    float4 Color     : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.ClipPos = mul(Proj, float4(input.Pos, 0.0f, 1.0f));
    output.Color = input.Color;

    return output;
}