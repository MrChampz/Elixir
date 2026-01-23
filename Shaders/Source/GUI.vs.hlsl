[[vk::binding(0, 0)]]
cbuffer cbPerFrame : register(b0)
{
    float2 RenderExtent;    // Render extent for coordinates conversion
    float2 Padding;         // Padding for alignment
}

struct VS_INPUT
{
    // Per vertex
    float2 Position : POSITION;                 // Screen-space position
    float2 TexCoord : TEXCOORD0;                // UV coordinates

    // Per quad
    float2 InstancePos  : INSTANCE_POSITION;    // Instance position in screen-space
    float2 InstanceSize : INSTANCE_SIZE;        // Instance size in screen-space
    float CornerRadius  : CORNER_RADIUS;        // Corner radius for rounded quad
    float Padding       : PADDING;              // Padding for alignment
    float4 Color        : COLOR0;               // Instance color
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;  // Clip-space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates

    float2 InstancePos  : INSTANCE_POSITION;    // Instance position in screen-space
    float2 InstanceSize : INSTANCE_SIZE;        // Instance size in screen-space
    float CornerRadius  : CORNER_RADIUS;        // Corner radius for rounded quad
    float4 Color        : COLOR0;               // Instance color
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // Convert from screen-space [0, RenderExtent] to clip-space [-1, 1]
    // Note: Y is flipped because screen coordinates have origin at top-left.
    float2 pos = input.Position * input.InstanceSize + input.InstancePos;
    output.Position.x = (pos.x / RenderExtent.x) * 2.0f - 1.0f;
    output.Position.y = (pos.y / RenderExtent.y) * 2.0f - 1.0f;
    output.Position.z = 0.0f;
    output.Position.w = 1.0f;

    output.TexCoord = input.TexCoord;
    output.InstancePos  = input.InstancePos;
    output.InstanceSize = input.InstanceSize;
    output.CornerRadius  = input.CornerRadius;
    output.Color    = input.Color;

    return output;
}