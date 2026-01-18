[[vk::binding(1, 0)]]
Texture2D atlas : register(t0);

[[vk::binding(1, 1)]]
SamplerState atlasSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_POSITION;  // Clip space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange(float2 texCoord)
{
    const float pxRange = 6.0;

    float2 dx = ddx(texCoord);
    float2 dy = ddy(texCoord);
    float2 duv = float2(length(dx), length(dy));
    float screenPx = max(duv.x, duv.y);

    // Get texture dimensions
    uint width, height;
    atlas.GetDimensions(width, height);

    return pxRange / (screenPx * width);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    // pxRange = 4
    // width, Height = 306
    float2 unitRange = float2(4.0 / 256.0, 4.0 / 256.0);

    // Sample all four channels (RGB = MSD, A = true SDF)
    float4 mtsdf = atlas.Sample(atlasSampler, input.TexCoord);

    float msdf = median(mtsdf.r, mtsdf.g, mtsdf.b);
    float2 screenTexSize = 1.0f / fwidth(input.TexCoord);
    float screenPxRange = max(0.5f * dot(unitRange, screenTexSize), 1.0f);
    float screenPxDistance = screenPxRange * (msdf - 0.5f);
    float alphaFill = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
    float4 color = float4(input.Color.rgb, 1.0);
    color.a *= alphaFill;
    return color;
}