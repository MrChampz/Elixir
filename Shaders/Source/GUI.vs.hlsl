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

    uint VertexId : SV_VertexID;
    uint InstanceId : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 ClipPos          : SV_POSITION;          // Clip-space position
    float2 LocalPos         : INSTANCE_POSITION;    // Quad position in EXPANDED local-space
    float2 ContentPos       : CONTENT_POSITION;     // Original content position (for distance calc)
    float2 ContentSize      : CONTENT_SIZE;         // Original content size (for distance calc)
    float2 TexCoord         : TEXCOORD0;            // UV coordinates
    float4 Border           : BORDER;               // Border
    float4 InsetShadow      : SHADOW0;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 DropShadow       : SHADOW1;              // Shadow offset (x, y), blur (z) and intensity (w)
    float4 Color            : COLOR0;               // Instance color
    float4 OutlineColor     : OUTLINE0;             // Outline color
    float  OutlineThickness : OUTLINE1;             // Outline thickness
    uint   TextureIndex     : TEXTURE;              // Texture index
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // Generate quad positions using bit manipulation
    // Positions: 0,0 -> 1,1 -> 1,0 -> 0,0 -> 0,1 -> 1,1
    // Indices:   0      1      2      3      4      5
    // x: 0, 1, 1, 0, 0, 1 = bits: 100110 = 0x26
    // y: 0, 1, 0, 0, 1, 1 = bits: 110010 = 0x32
    const uint xBits = 0x26;
    const uint yBits = 0x32;

    float2 normalizedPos;
    normalizedPos.x = float((xBits >> input.VertexId) & 1);
    normalizedPos.y = float((yBits >> input.VertexId) & 1);

    // Store original quad size for distance calculations
    output.ContentSize = input.Size;

    // Shadow expansion: offset + blur radius (multiply by ~3 for proper coverage)
    float2 shadowOffset = input.DropShadow.xy;
    float shadowBlur = input.DropShadow.z;
    float shadowIntensity = input.DropShadow.w;
    float shadowExpansion = 0.0f;

    if (shadowIntensity > 0.0f)
    {
        shadowExpansion = length(shadowOffset) + shadowBlur * 3.0;
    }

    // Total expansion is the maximum of all effects
    float expansion = max(input.OutlineThickness, shadowExpansion);

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

    // UV coordinates should map to the CONTENT area, not the expanded area
    output.TexCoord = normalizedPos;

    output.Border  = input.Border;
    output.InsetShadow  = input.InsetShadow;
    output.DropShadow  = input.DropShadow;
    output.Color = input.Color;
    output.OutlineColor = input.OutlineColor;
    output.OutlineThickness = input.OutlineThickness;
    output.TextureIndex = input.TextureIndex;

    return output;
}