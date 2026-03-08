#include "Scissor.hlsl"
#include "SDF.hlsl"

// Global bindless resources (binding 0 = cis, binding 1 = textures, binding 2 = samplers)
[[vk::binding(1, 1)]] // binding, set
Texture2D textures[] : register(t0);

[[vk::binding(1, 0)]]
SamplerState samplerState : register(s0);

struct WhiteTexturePushConstant
{
    uint WhiteTextureIndex;
};
[[vk::push_constant]] WhiteTexturePushConstant pcWhiteTexture;

struct PS_INPUT
{
    float4 ClipPos          : SV_POSITION;          // Clip-space position
    float2 LocalPos         : INSTANCE_POSITION;    // Quad position in EXPANDED local-space
    float2 ContentPos       : CONTENT_POSITION;     // Original content position (for distance calc)
    float2 ContentSize      : CONTENT_SIZE;         // Original content size (for distance calc)
    float2 TexCoord         : TEXCOORD0;            // UV coordinates
    // When texture is used, this represents the borders of 9-patch texture.
    // Border mapping = (left, top, right, bottom).
    // When not used, this represents the corner radius of the quad.
    // Corner Radius mapping = (top-left, top-right, bottom-right, bottom-left).
    float4 Border           : BORDER;
    float4 InsetShadow      : SHADOW0;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 DropShadow       : SHADOW1;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 Color            : COLOR0;               // Instance color
    float4 OutlineColor     : OUTLINE0;             // Outline color
    float  OutlineThickness : OUTLINE1;             // Outline thickness
    uint   TextureIndex     : TEXTURE;              // Texture index
    float4 ScissorRect      : SCISSOR;              // Scissor rect (x, y, width, height)
};

#define SCALE 6.0f // in pixels
#define DPI_SCALE 2.0f

#define SHADOW_COLOR float4(0, 0, 0, 1)

/**
 * Calculates texture coordinates based on UV coordinates and 9-patch borders.
 * texIndex: texture index in textures array
 * texCoords: input UV coordinates (0.0 - 1.0)
 * size: quad size in screen-space
 * border: left, top, right, bottom
 */
float2 calculateTexCoords(uint texIndex, float2 texCoords, float2 size, float4 border)
{
    // Get texture dimensions
    float2 texSize;
    textures[texIndex].GetDimensions(texSize.x, texSize.y);

    // Border values in texture pixels
    float texBorderLeft = border.x;
    float texBorderTop = border.y;
    float texBorderRight = border.z;
    float texBorderBottom = border.w;

    // Normalize borders with dpi scale
    float scale = SCALE * DPI_SCALE;

    // Scale borders proportionally to maintain appearance
    float leftBorder = texBorderLeft * (scale / texBorderLeft);
    float topBorder = texBorderTop * (scale / texBorderTop);
    float rightBorder = texBorderRight * (scale / texBorderRight);
    float bottomBorder = texBorderBottom * (scale / texBorderBottom);

    // Border in UV space
    float texLeft = texBorderLeft / texSize.x;
    float texRight = 1.0f - (texBorderRight / texSize.x);
    float texBottom = texBorderBottom / texSize.y;
    float texTop = 1.0f - (texBorderTop / texSize.y);

    // Calculate position in pixels
    float2 pixel = texCoords * size;

    // Remap uvs based on 9-patch regions
    float2 coords;

    // X-axis remapping
    float centerWidth = size.x - leftBorder - rightBorder;

    if (pixel.x < leftBorder)
    {
        // Left border region
        coords.x = (pixel.x / leftBorder) * texLeft;
    }
    else if (pixel.x > size.x - rightBorder)
    {
        // Right border region
        float offset = pixel.x - (size.x - rightBorder);
        coords.x = texRight + (offset / rightBorder) * (1.0f - texRight);
    }
    else
    {
        // Center region (stretched)
        float offset = pixel.x - leftBorder;
        coords.x = texLeft + (offset / centerWidth) * (texRight - texLeft);
    }

    // Y-axis remapping
    float centerHeight = size.y - bottomBorder - topBorder;

    if (pixel.y < bottomBorder)
    {
        // Bottom border region
        coords.y = (pixel.y / bottomBorder) * texBottom;
    }
    else if (pixel.y > size.y - topBorder)
    {
        // Top border region
        float offset = pixel.y - (size.y - topBorder);
        coords.y = texTop + (offset / topBorder) * (1.0f - texTop);
    }
    else
    {
        // Center region (stretched)
        float offset = pixel.y - bottomBorder;
        coords.y = texBottom + (offset / centerHeight) * (texTop - texBottom);
    }

    return saturate(coords);
}

/**
 * Applies a drop shadow effect to the quad.
 * color: previous color
 * shadow: shadow parameters (offset.x, offset.y, blur, intensity)
 * localPos: local-space position for distance calculation
 * halfSize: half of the quad size for distance calculation
 * cornerRadii: corner radii for distance calculation
 */
float4 applyShadow(float4 color, float4 shadow, float2 localPos, float2 halfSize, float4 cornerRadii)
{
    float4 finalColor = color;

    // Shadow parameters
    float2 offset = shadow.xy;
    float blur = shadow.z;
    float intensity = shadow.w;

    if (any(offset) && intensity > 0.0f)
    {
        // Calculate shadow distance (offset SDF)
        float2 shadowPos = localPos - offset;
        float dist = sdfRect(shadowPos, halfSize, cornerRadii);

        // Soft shadow with gaussian-like falloff
        float alpha = 1.0f - smoothstep(-blur, blur, dist);
        alpha = SHADOW_COLOR.a * alpha * intensity;

        finalColor = float4(SHADOW_COLOR.rgb, alpha);
    }

    return finalColor;
}

/**
 * Applies an outline effect to the quad.
 * color: previous color
 * thickness: outline thickness
 * outlineColor: outline color
 * sd: signed distance
 * px: pixel width (calculated as fwidth(sd))
 */
float4 applyOutline(float4 color, float thickness, float4 outlineColor, float sd, float px)
{
    float4 finalColor = color;

    if (thickness > 0.0f)
    {
        float outlineInner = smoothstep(-px, px, sd);
        float outlineOuter = smoothstep(thickness - px, thickness + px, sd);
        float outline = outlineInner * (1.0f - outlineOuter);
        //outline = smoothstep(thickness + px, thickness - px, abs(sd));

        finalColor.rgb = lerp(finalColor.rgb, outlineColor.rgb, outline);
        finalColor.a = max(finalColor.a, outline * outlineColor.a);
    }

    return finalColor;
}

/**
 * Applies an inset shadow effect to the quad.
 * color: previous color
 * shadow: shadow parameters (offset.x, offset.y, blur, intensity)
 * localPos: local-space position for distance calculation
 * halfSize: half of the quad size for distance calculation
 * cornerRadii: corner radii for distance calculation
 */
float4 applyInsetShadow(float4 color, float4 shadow, float2 localPos, float2 halfSize, float4 cornerRadii)
{
    float4 finalColor = color;

    // Shadow parameters
    float2 offset = shadow.xy;
    float blur = shadow.z;
    float intensity = shadow.w;

    if (any(offset) && intensity > 0.0f)
    {
        // Calculate shadow distance (offset SDF)
        float2 pos = localPos + (offset * -1.0f);
        float dist = sdfRect(pos, halfSize, cornerRadii);

        // Inner shadow only renders INSIDE the shape (dist < 0)
        // Fade from edge inward
        float mask = smoothstep(-blur, 0.0, dist);
        float alpha = SHADOW_COLOR.a * mask * intensity;

        // Darken the shape where inner shadow is
        finalColor = lerp(finalColor, SHADOW_COLOR, alpha);
    }

    return finalColor;
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

    float4 color = input.Color;
    float2 texCoords = input.TexCoord;

    if (input.TextureIndex > pcWhiteTexture.WhiteTextureIndex)
    {
        texCoords = calculateTexCoords(
            input.TextureIndex,
            input.TexCoord,
            input.ContentSize,
            input.Border
        );
    }

    float4 tex = textures[input.TextureIndex].Sample(samplerState, texCoords);
    color *= tex;
    if (input.TextureIndex > pcWhiteTexture.WhiteTextureIndex) return color;

    float2 halfSize = input.ContentSize * 0.5;
    float2 contentCenter = input.ContentPos + halfSize;
    float2 localPos = input.LocalPos - contentCenter;

    // Calculate the signed distance
    float dist = sdfRect(localPos, halfSize, input.Border);
	float px = fwidth(dist);

    // Start with transparent
    float4 finalColor = float4(0, 0, 0, 0);

    // Run these effects BEFORE shape rendering
    finalColor = applyShadow(finalColor, input.DropShadow, localPos, halfSize, input.Border);
    finalColor = applyOutline(finalColor, input.OutlineThickness, input.OutlineColor, dist, px);

    // Lerp final color with shape color based on actual quad shape
    float shapeMask = 1.0f - smoothstep(-px, px, dist);
    finalColor = lerp(finalColor, color, shapeMask);

    // Run these effects AFTER shape rendering
    if (dist < (-px + 1.0f))
    {
        finalColor = applyInsetShadow(finalColor, input.InsetShadow, localPos, halfSize, input.Border);
    }

    if (finalColor.a < 0.001) discard;
    return finalColor;
}