#include "Scissor.hlsl"
#include "SDF.hlsl"

// Global bindless resources (binding 1 = textures)
[[vk::binding(1, 1)]] // binding, set
Texture2D atlases[] : register(t0);

[[vk::binding(1, 0)]]
SamplerState atlasSampler : register(s0);

struct PS_INPUT
{
    float4 ClipPos            : SV_POSITION;          // Clip-space position
    float2 LocalPos           : INSTANCE_POSITION;    // Quad position in EXPANDED local-space
    float2 ContentPos         : CONTENT_POSITION;     // Original content position (for distance calc)
    float2 ContentSize        : CONTENT_SIZE;         // Original content size (for distance calc)
    float2 TexCoords          : TEXCOORD0;            // UV coordinates
    //float4 DropShadow       : SHADOW1;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 Color              : COLOR0;               // Instance color
    //float4 OutlineColor     : OUTLINE0;             // Outline color
    //float  OutlineThickness : OUTLINE1;             // Outline thickness
    uint   AtlasIndex         : TEXTURE;              // Atlas index in the texture set
    float2 UnitRange          : UNITRANGE;            // Unit range for the selected font (for distance calc)
    float4 ClipRect           : CLIP_RECT;            // (x, y, width, height)
    float4 RoundedClipRect    : ROUNDED_CLIP;         // (x, y, width, height)
    float4 ClipCornerRadius   : CLIP_RADIUS;          // (top-left, top-right, bottom-right, bottom-left)
};

float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float4 main(PS_INPUT input) : SV_TARGET
{
    // Discard pixels outside the scissor rect
    if (IsClipRectValid(input.ClipRect) &&
       (input.ClipPos.x < input.ClipRect.x ||
        input.ClipPos.y < input.ClipRect.y ||
        input.ClipPos.x > input.ClipRect.x + input.ClipRect.z ||
        input.ClipPos.y > input.ClipRect.y + input.ClipRect.w))
        discard;

    if (input.RoundedClipRect.z >= 0 && input.RoundedClipRect.w >= 0)
    {
        float2 halfSize = input.RoundedClipRect.zw * 0.5;
        float2 position = input.ClipPos.xy - (input.RoundedClipRect.xy + halfSize);
        clip(-SDFRect(position, halfSize, input.ClipCornerRadius));
    }

    // Sample MTSDF atlas: RGB = multi-channel SDF, A = true SDF
    float4 atlas = atlases[input.AtlasIndex].Sample(atlasSampler, input.TexCoords);

    // Median-of-three reconstructs sharp corners from the multi-channel encoding
    float sd = Median(atlas.r, atlas.g, atlas.b);

    // Screen-space derivative of distance for resolution-independent anti-aliasing
    float screenPxDistance = fwidth(sd);

    // 0.5 = glyph boundary in the distance field
    float alpha = smoothstep(0.5f - screenPxDistance, 0.5f + screenPxDistance, sd);
    alpha *= input.Color.a;

    return float4(input.Color.rgb, alpha);
}