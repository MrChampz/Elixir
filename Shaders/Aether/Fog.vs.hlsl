// Fullscreen triangle for the fog post pass.

struct VSOutput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.UV = uv;
    output.Pos = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}
