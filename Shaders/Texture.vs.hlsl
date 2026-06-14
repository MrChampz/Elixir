[[vk::binding(0, 0)]]
cbuffer frameBuffer : register(b0)
{
    float4x4 WorldViewProjectionMatrix;
}

struct VSInput
{
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD;
};

struct VSOutput
{
    float4 Pos : SV_Position;
    float2 Tex : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Transform the vertex position from model space to clip space using the matrix.
    output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProjectionMatrix);

    // Pass the texture coordinate directly to the pixel shader.
    output.Tex = input.Tex;

    return output;
}