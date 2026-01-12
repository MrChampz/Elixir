[[vk::binding(0, 0)]]
cbuffer cbPerFrame : register(b0)
{
    float2 RenderExtent;    // Render extent for coordinates conversion
    float2 Padding;         // Padding for alignment
}

struct VS_INPUT
{
    float2 Position : POSITION;     // Screen-space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;  // Clip-space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // Convert from screen-space [0, RenderExtent] to clip-space [-1, 1]
    // Note: Y is flipped because screen coordinates have origin at top-left.
    output.Position.x = (input.Position.x / RenderExtent.x) * 2.0f - 1.0f;
    output.Position.y = (input.Position.y / RenderExtent.y) * 2.0f - 1.0f;
    output.Position.z = 0.0f;
    output.Position.w = 1.0f;

    output.TexCoord = input.TexCoord;
    output.Color    = input.Color;

    return output;
}