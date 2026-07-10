// Global bindless resources (binding 0 = cis, binding 1 = textures, binding 2 = samplers)
[[vk::binding(1, 1)]]
Texture2D sprites[] : register(t0);

[[vk::binding(1, 0)]]
SamplerState spriteSampler : register(s0);

struct SpritePushConstants
{
    uint SpriteIndex;
    uint FlipCols;
    uint FlipRows;
    uint FlipFrames;
    uint FlipBlend;
};
[[vk::push_constant]]
SpritePushConstants pc;

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float4 Color   : COLOR;
    float2 CellA   : TEXCOORD0;
    float2 CellB   : TEXCOORD1;
    float2 QuadUV  : TEXCOORD2;
    float  Blend   : TEXCOORD3;
};

// Build the atlas UV for one cell, clamped to its interior by half a texel so a
// bilinear tap at the cell edge cannot sample the adjacent frame.
float2 CellSampleUV(float2 cell, float2 quadUV, float2 invDims, float2 halfTexel)
{
    float2 cellMin = cell * invDims;
    float2 uv = cellMin + quadUV * invDims;
    return clamp(uv, cellMin + halfTexel, cellMin + invDims - halfTexel);
}

float4 main(PSInput input) : SV_Target0
{
    uint cols = max(pc.FlipCols, 1u);
    uint rows = max(pc.FlipRows, 1u);

    uint texW, texH;
    sprites[pc.SpriteIndex].GetDimensions(texW, texH);

    float2 invDims = 1.0f / float2((float)cols, (float)rows);
    float2 halfTexel = 0.5f / float2((float)texW, (float)texH);

    float2 uvA = CellSampleUV(input.CellA, input.QuadUV, invDims, halfTexel);
    float2 uvB = CellSampleUV(input.CellB, input.QuadUV, invDims, halfTexel);

    float4 frameA = sprites[pc.SpriteIndex].Sample(spriteSampler, uvA);
    float4 frameB = sprites[pc.SpriteIndex].Sample(spriteSampler, uvB);
    float4 sprite = lerp(frameA, frameB, input.Blend);

    float3 color = input.Color.rgb * sprite.rgb;
    float alpha = input.Color.a * sprite.a;

    return float4(color, alpha);
}