static const float4 vertices[3] = {
    float4(-0.5f,  0.5f,    0.0f, 1.0f),
    float4( 0.0f, -0.5f,    0.5f, 0.0f),
    float4( 0.5f,  0.5f,    1.0f, 1.0f)
};

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 ViewProj;
}

struct VSOutput
{
    float4 Pos : SV_Position;
    float2 Tex : TEXCOORD;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    float4 vertex = vertices[vertexId];

    VSOutput output;
    output.Pos = mul(ViewProj, float4(vertex.xy, 0.0f, 1.0f));
    output.Tex = vertex.zw;

    return output;
}