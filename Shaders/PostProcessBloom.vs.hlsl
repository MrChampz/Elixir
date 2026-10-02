struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

VSOutput main(const uint vertexIndex : SV_VertexID)
{
    const float2 uv = float2(
        float((vertexIndex << 1u) & 2u),
        float(vertexIndex & 2u)
    );

    VSOutput output;
    output.Position = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    output.UV = uv;
    return output;
}
