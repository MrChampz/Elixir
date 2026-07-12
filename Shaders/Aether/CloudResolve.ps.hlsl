// Cloud temporal resolve (TAA). Blends the current half-res raymarch (upscaled)
// with the reprojected history so the animated per-frame dither averages out to
// a clean, grain-free result over a few frames. Reprojection uses the previous
// frame's view-projection on the ray's far point (clouds are distant, so this
// tracks camera rotation well). A neighbourhood colour clamp on the history
// suppresses ghosting when the reprojection is wrong (disocclusion / fast turn).

[[vk::binding(0, 0)]]
cbuffer cbCloud : register(b0)
{
    float4x4 InvViewProj;
    float4 CameraPos;
    float4 SunDir;
    float4 SunColor;
    float4 SkyZenith;
    float4 SkyHorizon;
    float4 CloudParams;
    float4 CloudParams2;
    float4 CloudParams3;
    float4x4 PrevViewProj;
    float4 TemporalParams; // x=FrameIndex, y=BlendAlpha, z=HistoryValid
};

[[vk::binding(0, 1)]]
Texture2D cloudCurrent : register(t0);

[[vk::binding(1, 1)]]
Texture2D cloudHistory : register(t1);

[[vk::binding(2, 1)]]
SamplerState linearSampler : register(s0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    float4 current = cloudCurrent.Sample(linearSampler, input.UV);

    // Reproject this pixel into the previous frame. Take a far point along the
    // current view ray and project it with the previous view-projection.
    float2 ndc = input.UV * 2.0f - 1.0f;
    float4 farPoint = mul(InvViewProj, float4(ndc, 1.0f, 1.0f));
    float3 world = farPoint.xyz / farPoint.w;

    float4 prevClip = mul(PrevViewProj, float4(world, 1.0f));
    float2 prevUV = (prevClip.xy / prevClip.w) * 0.5f + 0.5f;

    bool onScreen = all(prevUV == saturate(prevUV)) && prevClip.w > 0.0f;
    bool valid = TemporalParams.z > 0.5f && onScreen;
    if (!valid)
        return current;

    float4 history = cloudHistory.Sample(linearSampler, prevUV);

    // Neighbourhood min/max of the current frame -> clamp the history to it so a
    // stale/mis-reprojected sample can't ghost.
    float2 texel = fwidth(input.UV);
    float4 lo = current;
    float4 hi = current;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            float4 s = cloudCurrent.Sample(linearSampler, input.UV + float2(x, y) * texel);
            lo = min(lo, s);
            hi = max(hi, s);
        }
    }
    history = clamp(history, lo, hi);

    // Exponential accumulation: keep mostly history, fold in a little current.
    float alpha = TemporalParams.y;
    return lerp(history, current, alpha);
}
