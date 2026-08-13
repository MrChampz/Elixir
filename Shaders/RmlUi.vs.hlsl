[[vk::binding(0, 0)]]
cbuffer cbRmlUiFrame : register(b0)
{
    float2 Viewport;
}

struct VS_INPUT
{
    float2 Position : POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    uint TextureIndex : TEXTURE;
};

struct VS_OUTPUT
{
    float4 ClipPosition : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    nointerpolation uint TextureIndex : TEXTURE;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.ClipPosition = float4(
        input.Position.x / Viewport.x * 2.0f - 1.0f,
        input.Position.y / Viewport.y * 2.0f - 1.0f,
        0.0f,
        1.0f
    );
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    output.TextureIndex = input.TextureIndex;
    return output;
}
