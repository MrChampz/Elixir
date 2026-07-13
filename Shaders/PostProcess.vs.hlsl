// Fullscreen triangle: covers the screen from a single Draw(3), no vertex buffer.
struct VSOutput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((id << 1) & 2, id & 2); // (0,0), (2,0), (0,2)
    output.UV = uv;
    // Vulkan NDC Y points down, so UV.y == 0 maps to clip Y -1 (top) with no flip,
    // matching the render-target texture whose row 0 is the top.
    output.Pos = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}
