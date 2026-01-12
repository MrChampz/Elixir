struct VS_INPUT
{
    float2 Position : POSITION;     // NDC position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;  // Clip space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.Position = float4(input.Position, 0.0f, 1.0f);
    output.TexCoord = input.TexCoord;
    output.Color    = input.Color;

    return output;
}