struct PS_INPUT
{
    float4 Position : SV_POSITION;  // Clip space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates

    float2 InstancePos  : INSTANCE_POSITION;    // Instance position in screen-space
    float2 InstanceSize : INSTANCE_SIZE;        // Instance size in screen-space
    float CornerRadius  : CORNER_RADIUS;        // Corner radius for rounded quad
    float4 Color        : COLOR0;               // Instance color
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}