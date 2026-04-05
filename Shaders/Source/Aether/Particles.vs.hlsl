[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 ViewProj;
}

struct VSInput
{
    float2 Position      : POSITION;
    float4 Color         : COLOR;
    float  Size          : SIZE;
};

struct VSOutput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;

    [[vk::builtin("PointSize")]]
    float Size           : PSIZE;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 pos = float4(input.Position, 0.0f, 1.0f);
    output.ClipPos = mul(ViewProj, pos);

    output.Color = input.Color;
    output.Size = input.Size;

    return output;
}