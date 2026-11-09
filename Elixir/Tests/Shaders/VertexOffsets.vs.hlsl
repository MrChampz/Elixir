struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
};

VS_OUTPUT main(float4 color : COLOR0, uint vertex : SV_VertexID)
{
    VS_OUTPUT output;
    float2 position = float2((vertex << 1) & 2, vertex & 2);
    output.Position = float4(position * 2 - 1, 0, 1);
    output.Color = color;
    return output;
}
