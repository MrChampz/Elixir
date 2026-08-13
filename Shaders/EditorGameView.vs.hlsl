[[vk::binding(0, 0)]]
cbuffer cbGameViewFrame : register(b0)
{
    float2 Viewport;
    float AspectRatio;
    float Padding;
}

static const float2 positions[6] = {
    float2(-1.0f, -1.0f),
    float2( 3.0f, -1.0f),
    float2(-1.0f,  3.0f),
    float2( 0.0f, -0.58f),
    float2(-0.48f,  0.42f),
    float2( 0.48f,  0.42f)
};

static const float4 colors[6] = {
    float4(0.008f, 0.012f, 0.020f, 1.0f),
    float4(0.008f, 0.012f, 0.020f, 1.0f),
    float4(0.008f, 0.012f, 0.020f, 1.0f),
    float4(0.95f, 0.24f, 0.22f, 1.0f),
    float4(0.20f, 0.82f, 0.46f, 1.0f),
    float4(0.20f, 0.52f, 0.96f, 1.0f)
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float2 position = positions[vertexId];
    if (vertexId >= 3)
        position.x /= AspectRatio;

    output.Position = float4(position, 0.0f, 1.0f);
    output.Color = colors[vertexId];
    return output;
}
