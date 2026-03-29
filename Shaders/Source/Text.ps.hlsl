#include "Scissor.hlsl"

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
    float4 ScissorRect        : SCISSOR;              // Scissor rect (x, y, width, height)
};

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float4 main(PS_INPUT input) : SV_TARGET
{
    // Discard pixels outside the scissor rect
    if (isScissorRectValid(input.ScissorRect) &&
       (input.ClipPos.x < input.ScissorRect.x ||
        input.ClipPos.y < input.ScissorRect.y ||
        input.ClipPos.x > input.ScissorRect.x + input.ScissorRect.z ||
        input.ClipPos.y > input.ScissorRect.y + input.ScissorRect.w))
    {
        discard;
    }

    // Sample MTSDF atlas: RGB = multi-channel SDF, A = true SDF
    float4 atlas = atlases[input.AtlasIndex].Sample(atlasSampler, input.TexCoords);

    // Median-of-three reconstructs sharp corners from the multi-channel encoding
    float sd = median(atlas.r, atlas.g, atlas.b);

    // Screen-space derivative of distance for resolution-independent anti-aliasing
    float screenPxDistance = fwidth(sd);

    // 0.5 = glyph boundary in the distance field
    float alpha = smoothstep(0.5f - screenPxDistance, 0.5f + screenPxDistance, sd);
    alpha *= input.Color.a;

    return float4(input.Color.rgb, alpha);
}