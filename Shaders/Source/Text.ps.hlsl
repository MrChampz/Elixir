[[vk::binding(1, 0)]]
cbuffer cbFont : register(b0)
{
    float2 UnitRange;    // Calculated unit range for selected font
    float FontSize;      // Font size in pixels
    float Padding;       // Padding for alignment
}

[[vk::binding(2, 0)]]
Texture2D mtsdf : register(t0);

[[vk::binding(2, 1)]]
SamplerState atlasSampler : register(s0);

struct PS_INPUT
{
    float4 ClipPos          : SV_POSITION;          // Clip-space position
    float2 LocalPos         : INSTANCE_POSITION;    // Quad position in EXPANDED local-space
    float2 ContentPos       : CONTENT_POSITION;     // Original content position (for distance calc)
    float2 ContentSize      : CONTENT_SIZE;         // Original content size (for distance calc)
    float2 TexCoords        : TEXCOORD0;            // UV coordinates
    //float4 DropShadow       : SHADOW1;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 Color            : COLOR0;               // Instance color
    //float4 OutlineColor     : OUTLINE0;             // Outline color
    //float  OutlineThickness : OUTLINE1;             // Outline thickness
    //uint   TextureIndex     : TEXTURE;              // Texture index
};

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange(float2 texCoord)
{
    float2 screenTexSize = 1.0f / fwidth(texCoord);
    return max(0.5f * dot(UnitRange, screenTexSize), 1.0f);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float alpha = 0.0f;

    // Sample all four channels (RGB = MSD, A = true SDF)
    float4 atlas = mtsdf.Sample(atlasSampler, input.TexCoords);
    float msdf = median(atlas.r, atlas.g, atlas.b);
    float sdf = atlas.a;

    // MTSDF blending
    float sd = lerp(sdf, msdf, saturate(2.0f * abs(msdf - 0.5f)));

    // Screen space derivative of distance
    // float2 texelDensity = 1.0f / fwidth(input.TexCoord);
    // float pxRange = max(0.5f * dot(UnitRange, texelDensity), 1.0f);
    float pxRange = screenPxRange(input.TexCoords);

    // Final alpha
    float pxDistance = (sd - 0.5f) * pxRange;
    alpha = saturate(pxDistance + 0.5f);
    alpha *= input.Color.a;

    return float4(input.Color.rgb, alpha);
}