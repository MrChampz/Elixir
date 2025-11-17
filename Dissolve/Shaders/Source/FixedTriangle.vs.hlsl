static const float2 positions[3] = {
    float2( 0.0f, -0.5f),  // bottom
    float2( 0.5f,  0.5f),  // right
    float2(-0.5f,  0.5f)   // left
};

struct VSOutput
{
    float4 Pos : SV_Position;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    float2 pos = positions[vertexId];
    output.Pos = float4(pos, 0.0f, 1.0f);

    return output;
}