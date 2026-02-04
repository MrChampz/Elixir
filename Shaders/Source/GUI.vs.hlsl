[[vk::binding(0, 0)]]
cbuffer cbPerFrame : register(b0)
{
    float2 RenderExtent;    // Render extent for coordinates conversion
    float2 Padding;         // Padding for alignment
}

struct VS_INPUT
{
    // Per quad
    float2 Position     : INSTANCE_POSITION;    // Instance position in screen-space
    float2 Size         : INSTANCE_SIZE;        // Instance size in screen-space
    float4 CornerRadius : CORNER_RADIUS;        // Corner radius (top-left, top-right, bottom-right, bottom-left)
    float4 Color        : COLOR0;               // Instance color
    uint   TextureIndex : TEXTURE;              // Texture index

    uint VertexId : SV_VertexID;
    uint InstanceId : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 ClipPos      : SV_POSITION;          // Clip-space position
    float2 LocalPos     : INSTANCE_POSITION;    // Quad position in local-space
    float2 Size         : INSTANCE_SIZE;        // Quad size in screen-space
    float2 TexCoord     : TEXCOORD0;            // UV coordinates
    float4 CornerRadius : CORNER_RADIUS;        // Corner radius (top-left, top-right, bottom-right, bottom-left)
    float4 Color        : COLOR0;               // Instance color
    uint   TextureIndex : TEXTURE;              // Texture index
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

    // Calculate local-space position
    output.LocalPos = normalizedPos * input.Size;

    // Calculate screen-space position
    float2 screenPos = output.LocalPos + input.Position;

    // Convert from screen-space [0, RenderExtent] to clip-space [-1, 1]
    // Note: Y is flipped because screen coordinates have origin at top-left.
    output.ClipPos.x = (screenPos.x / RenderExtent.x) * 2.0f - 1.0f;
    output.ClipPos.y = (screenPos.y / RenderExtent.y) * 2.0f - 1.0f;
    output.ClipPos.z = 0.0f;
    output.ClipPos.w = 1.0f;

    output.Size = input.Size;
    output.TexCoord = normalizedPos;
    output.CornerRadius  = input.CornerRadius;
    output.Color = input.Color;
    output.TextureIndex = input.TextureIndex;

    return output;
}