[[vk::binding(0, 0)]]
cbuffer cbPerFrame : register(b0)
{
    float2 RenderExtent;    // Render extent for coordinates conversion
    float2 Padding;         // Padding for alignment
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

    // Convert from screen-space [0, RenderExtent] to clip-space [-1, 1]
    // Note: Y is flipped because screen coordinates have origin at top-left.
    output.ClipPos.x = (input.Pos.x / RenderExtent.x) * 2.0f - 1.0f;
    output.ClipPos.y = (input.Pos.y / RenderExtent.y) * 2.0f - 1.0f;
    output.ClipPos.z = 0.0f;
    output.ClipPos.w = 1.0f;

    output.Color = input.Color;

    return output;
}