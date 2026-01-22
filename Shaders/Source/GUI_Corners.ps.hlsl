struct PS_INPUT
{
    float4 Position : SV_POSITION;  // Clip space position
    float2 TexCoord : TEXCOORD0;    // UV coordinates
    float4 Color    : COLOR0;       // Vertex color
};

float sdRoundedBox(float2 p, float2 b, float r)
{
    float2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float cornerRadius = 10.0;

    float2 center = float2(120.0f, 40.0f) * 0.5;
    float2 halfSize = center;
    float2 localPos = input.Position.xy - center;

    // Calculate distance to rounded rectangle
    float dist = sdRoundedBox(localPos, halfSize, cornerRadius);

    // Anti-aliased edge
    float alpha = 1.0f - smoothstep(-1.0f, 1.0f, dist);

    float4 color = input.Color;
    color.a *= alpha;

    return color;
}