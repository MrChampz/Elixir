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
    uint GradientIndex;
    uint GradientMode;
    uint NormalIndex;
    uint NormalMode;
    uint EmissionIndex;
    float EmissionScale;
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

    float3 rgb = sprite.rgb;

    // Gradient/blackbody remap: use the sheet's luminance as a lookup into a 1D
    // ramp (sampled along U). Clamp to the LUT interior so bilinear does not wrap
    // past the ends (the shared sampler uses Repeat addressing).
    if (pc.GradientMode != 0u)
    {
        float t = dot(sprite.rgb, float3(0.299f, 0.587f, 0.114f));

        uint lutW, lutH;
        sprites[pc.GradientIndex].GetDimensions(lutW, lutH);
        float halfTexel = 0.5f / (float)lutW;
        t = clamp(t, halfTexel, 1.0f - halfTexel);

        rgb = sprites[pc.GradientIndex].Sample(spriteSampler, float2(t, 0.5f)).rgb;
    }

    // Fake-normal lighting: a radial normal map lets the flat billboard read as a
    // lit volume (self-shadowing). Sampled at the sprite-local UV (not the atlas
    // cell) since the normal map is not a flipbook.
    if (pc.NormalMode != 0u)
    {
        float3 n = normalize(sprites[pc.NormalIndex].Sample(spriteSampler, input.QuadUV).xyz * 2.0f - 1.0f);
        float3 lightDir = normalize(float3(-0.4f, 0.6f, 0.7f)); // billboard space: upper-left, toward camera
        float ndl = saturate(dot(n, lightDir));
        const float ambient = 0.35f;
        rgb *= ambient + (1.0f - ambient) * ndl;
    }

    float3 color = input.Color.rgb * rgb;

    // Emission map: a second flipbook (same cells) added on top for baked
    // fire-in-smoke. Scaled by the particle alpha so it fades with the sprite.
    if (pc.EmissionScale > 0.0f)
    {
        float3 emA = sprites[pc.EmissionIndex].Sample(spriteSampler, uvA).rgb;
        float3 emB = sprites[pc.EmissionIndex].Sample(spriteSampler, uvB).rgb;
        float3 emission = lerp(emA, emB, input.Blend);
        color += emission * pc.EmissionScale * input.Color.a;
    }

    float alpha = input.Color.a * sprite.a;

    return float4(color, alpha);
}