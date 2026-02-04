[[vk::binding(0, 1)]] // binding, set
Texture2D textures[] : register(t0);

[[vk::binding(1, 1)]]
SamplerState samplerState : register(s0);

struct PS_INPUT
{
    float4 ClipPos      : SV_POSITION;          // Clip-space position
    float2 LocalPos     : INSTANCE_POSITION;    // Quad position in local-space
    float2 Size         : INSTANCE_SIZE;        // Quad size in screen-space
    float2 TexCoord     : TEXCOORD0;            // UV coordinates
    float4 CornerRadius : CORNER_RADIUS;        // Corner radius (top-left, top-right, bottom-right, bottom-left)
    float4 Color        : COLOR0;               // Instance color
    uint   TextureIndex : TEXTURE;              // Texture index
};

float sdRoundedBox(float2 p, float2 b, float4 r)
{
    // Default: top-left
    float radius = r.x;

    // Determine corner radius based on quadrant
    if (p.x >= 0.0 && p.y >= 0.0)
    {
        radius = r.y; // top-right
    }
    else if (p.x >= 0.0 && p.y < 0.0)
    {
        radius = r.z; // bottom-right
    }
    else if (p.x < 0.0 && p.y < 0.0)
    {
        radius = r.w; // bottom-left
    }
    else
    {
        radius = r.x; // top-left
    }

    float2 q = abs(p) - b + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

/**
 * Applies rounded corners to a quad.
 * pos: local-space quad position
 * size: quad size in screen-space
 * color: quad color
 * cornerRadius: top-left, top-right, bottom-right, bottom-left
 */
float4 applyCornerRadius(float2 pos, float2 size, float4 color, float4 cornerRadius)
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

    float4 tex = textures[input.TextureIndex].Sample(samplerState, input.TexCoord);
    color *= tex;
    if (color.a == 0.0f) discard;

    if (dot(input.CornerRadius, float4(1, 1, 1, 1)) > 0.0f)
    {
        color = applyCornerRadius(input.LocalPos, input.Size, color, input.CornerRadius);
    }

    return color;
}