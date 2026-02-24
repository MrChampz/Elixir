#include "Quad.hlsl"

[[vk::binding(0, 0)]]
cbuffer cbPerFrame : register(b0)
{
    float2 RenderExtent;    // Render extent for coordinates conversion
    float2 Padding;         // Padding for alignment
}

struct VS_INPUT
{
    // Per quad
    float2 Position         : INSTANCE_POSITION;    // Instance position in screen-space
    float2 Size             : INSTANCE_SIZE;        // Instance size in screen-space
    float4 TexCoords        : TEXCOORD0;            // Instance UV coordinates; offset(x, y), size (z, w)
    //float4 DropShadow       : SHADOW1;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 Color            : COLOR0;               // Instance color
    //float4 OutlineColor     : OUTLINE0;             // Outline color
    //float  OutlineThickness : OUTLINE1;             // Outline thickness
    //uint   TextureIndex     : TEXTURE;              // Texture index

    uint VertexId : SV_VertexID;
    uint InstanceId : SV_InstanceID;
};

struct VS_OUTPUT
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

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // Generate quad positions using bit manipulation
    float2 normalizedPos = CalculateQuadPosition(input.VertexId);

    // Store original quad size for distance calculations
    output.ContentSize = input.Size;

    // Shadow expansion: offset + blur radius (multiply by ~3 for proper coverage)
    //float2 shadowOffset = input.DropShadow.xy;
    //float shadowBlur = input.DropShadow.z;
    //float shadowIntensity = input.DropShadow.w;
    float shadowExpansion = 0.0f;

    //if (shadowIntensity > 0.0f)
    //{
    //    shadowExpansion = length(shadowOffset) + shadowBlur * 3.0;
    //}

    // Total expansion is the maximum of all effects
    float expansion = 0.0;//max(input.OutlineThickness, shadowExpansion);

    // Expand the quad geometry
    float2 expandedSize = input.Size + float2(expansion * 2.0, expansion * 2.0);

    // Calculate local-space position in EXPANDED quad
    output.LocalPos = normalizedPos * expandedSize;

    // Offset to account for expansion (center the content)
    float2 expansionOffset = float2(expansion, expansion);

    // Store the content position within the expanded quad
    // This is where the actual content starts
    output.ContentPos = expansionOffset;

    // Calculate screen-space position with expansion
    float2 screenPos = output.LocalPos + (input.Position - expansionOffset);

    // Convert from screen-space [0, RenderExtent] to clip-space [-1, 1]
    // Note: Y is flipped because screen coordinates have origin at top-left.
    output.ClipPos.x = (screenPos.x / RenderExtent.x) * 2.0f - 1.0f;
    output.ClipPos.y = (screenPos.y / RenderExtent.y) * 2.0f - 1.0f;
    output.ClipPos.z = 0.0f;
    output.ClipPos.w = 1.0f;

    // Calculate UV coordinates for the expanded quad
    float2 uv = float2(normalizedPos.x, 1.0f - normalizedPos.y);
    float2 uvMin = input.TexCoords.xy;
    float2 uvMax = input.TexCoords.zw;
    output.TexCoords = lerp(uvMin, uvMax, uv);

    //output.DropShadow  = input.DropShadow;
    output.Color = input.Color;
    //output.OutlineColor = input.OutlineColor;
    //output.OutlineThickness = input.OutlineThickness;
    //output.TextureIndex = input.TextureIndex;

    return output;
}