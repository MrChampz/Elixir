// Single-pass bloom: extract the bright part of the scene-color snapshot, blur
// it with a small tap disk, and output it for additive compositing onto the
// render target. (LDR: blooms whatever is already bright, e.g. an additive fire
// core that saturates toward white.)

[[vk::binding(0, 1)]]
Texture2D sceneColor : register(t0);

[[vk::binding(1, 1)]]
SamplerState bloomSampler : register(s0);

struct BloomPushConstants
{
    float Threshold;
    float Intensity;
    float Radius; // base tap spacing in texels
    float ViewW;
    float ViewH;
};
[[vk::push_constant]]
BloomPushConstants pc;

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

float3 BrightPass(float3 c)
{
    float luma = dot(c, float3(0.299f, 0.587f, 0.114f));
    float excess = max(luma - pc.Threshold, 0.0f);
    return c * (excess / max(luma, 1e-4f));
}

float3 SampleBright(float2 uv)
{
    return BrightPass(sceneColor.Sample(bloomSampler, saturate(uv)).rgb);
}

float4 main(PSInput input) : SV_Target0
{
    float2 texel = float2(1.0f / pc.ViewW, 1.0f / pc.ViewH);

    float3 sum = SampleBright(input.UV);
    float wsum = 1.0f;

    const int RINGS = 3;
    const int SPOKES = 8;

    [unroll] for (int r = 1; r <= RINGS; ++r)
    {
        float radius = pc.Radius * (float)r;
        float weight = 1.0f / ((float)r + 1.0f);

        [unroll] for (int s = 0; s < SPOKES; ++s)
        {
            float ang = 6.2831853f * ((float)s / (float)SPOKES);
            float2 off = float2(cos(ang), sin(ang)) * texel * radius;
            sum += SampleBright(input.UV + off) * weight;
            wsum += weight;
        }
    }

    float3 bloom = (sum / wsum) * pc.Intensity;
    return float4(bloom, 1.0f);
}
