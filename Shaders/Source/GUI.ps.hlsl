struct PS_INPUT
{
    float4 ClipPos      : SV_POSITION;          // Clip-space position
    float2 LocalPos     : INSTANCE_POSITION;    // Quad position in local-space
    float2 Size         : INSTANCE_SIZE;        // Quad size in screen-space
    float2 TexCoord     : TEXCOORD0;            // UV coordinates
    float CornerRadius  : CORNER_RADIUS;        // Corner radius for rounded quadv
    float4 Color        : COLOR0;               // Instance color
};

float sdRoundedBox(float2 p, float2 b, float r)
{
    float2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float4 applyCornerRadius(float2 pos, float2 size, float4 color, float cornerRadius)
{
    float2 center = size * 0.5;
    float2 halfSize = center;
    float2 localPos = pos - center;

    // Calculate distance to rounded rectangle
    float dist = sdRoundedBox(localPos, halfSize, cornerRadius);

    // Anti-aliased edge
    float alpha = 1.0f - smoothstep(-1.0f, 1.0f, dist);

    color.a *= alpha;
    return color;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = input.Color;
    if (color.a == 0.0f) discard;

    if (input.CornerRadius > 0.0f)
    {
        color = applyCornerRadius(input.LocalPos, input.Size, color, input.CornerRadius);
    }

    return color;
}