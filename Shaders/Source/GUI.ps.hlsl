struct PS_INPUT
{
    float4 Position : SV_POSITION;  // Clip space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}