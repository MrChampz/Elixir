[[vk::binding(1, 0)]]
cbuffer cbFont : register(b0)
{
    float2 UnitRange;    // Calculated unit range for selected font
    float FontSize;      // Font size in pixels
    float Padding;       // Padding for alignment
}

[[vk::binding(2, 0)]]
Texture2D hardmask : register(t0);

[[vk::binding(2, 1)]]
Texture2D mtsdf : register(t1);

[[vk::binding(2, 2)]]
SamplerState atlasSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_POSITION;  // Clip space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
};

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

// float screenPxRange(float2 texCoord)
// {
//     const float pxRange = 6.0;
//
//     float2 dx = ddx(texCoord);
//     float2 dy = ddy(texCoord);
//     float2 duv = float2(length(dx), length(dy));
//     float screenPx = max(duv.x, duv.y);
//
//     // Get texture dimensions
//     uint width, height;
//     atlas.GetDimensions(width, height);
//
//     return pxRange / (screenPx * width);
// }

float screenPxRange(float2 texCoord)
{
    float2 screenTexSize = 1.0f / fwidth(texCoord);
    return max(0.5f * dot(UnitRange, screenTexSize), 1.0f);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float alpha = 0.0f;

    if (FontSize > 18.0f)
    {
        // Sample all four channels (RGB = MSD, A = true SDF)
        float4 atlas = mtsdf.Sample(atlasSampler, input.TexCoord);
        float msdf = median(atlas.r, atlas.g, atlas.b);
        float sdf = atlas.a;

        // MTSDF blending
        float sd = lerp(sdf, msdf, saturate(2.0f * abs(msdf - 0.5f)));

        // Screen space derivative of distance
        // float2 texelDensity = 1.0f / fwidth(input.TexCoord);
        // float pxRange = max(0.5f * dot(UnitRange, texelDensity), 1.0f);
        float pxRange = screenPxRange(input.TexCoord);

        // Final alpha
        float pxDistance = (sd - 0.5f) * pxRange;
        alpha = saturate(pxDistance + 0.5f);
    }
    else
    {
        alpha = hardmask.Sample(atlasSampler, input.TexCoord).r;
    }

    return float4(1.0f, 1.0f, 1.0f, alpha);
}