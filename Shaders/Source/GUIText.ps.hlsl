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
    // Sample all four channels (RGB = MSD, A = true SDF)
    float4 mtsdf = atlas.Sample(atlasSampler, input.TexCoord);

    float msdf = median(mtsdf.r, mtsdf.g, mtsdf.b);

    // Get texture dimensions
    uint width, height;
    atlas.GetDimensions(width, height);

    float dx = ddx(input.TexCoord.x) * width;
    float dy = ddy(input.TexCoord.y) * height;

    float w = fwidth(mtsdf.a);
    float opacity = smoothstep(0.5 - w, 0.5 + w, mtsdf.a);

    return float4(input.Color.rgb, mtsdf.r);
}