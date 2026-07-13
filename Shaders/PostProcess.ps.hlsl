// Post-processing: approximate bloom around bright areas + FXAA edge anti-aliasing.
// Reads the grabbed scene colour and writes the composited result back.

[[vk::binding(0, 0)]]
cbuffer cbPost : register(b0)
{
    float2 ScreenSize;
    float BloomThreshold;
    float BloomIntensity;
    float BloomRadius; // in texels
    float _Pad0;
    float _Pad1;
    float _Pad2;
};

[[vk::binding(0, 1)]]
Texture2D sceneTexture : register(t0);

[[vk::binding(1, 1)]]
SamplerState postSampler : register(s0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

static const float3 LUMA = float3(0.299f, 0.587f, 0.114f);

float3 SampleScene(float2 uv)
{
    return sceneTexture.Sample(postSampler, saturate(uv)).rgb;
}

// Compact FXAA (Timothy Lottes' console variant): find the local edge direction
// from luma and blend two taps along it.
float3 FXAA(float2 uv, float2 texel)
{
    float3 nw = SampleScene(uv + float2(-1, -1) * texel);
    float3 ne = SampleScene(uv + float2( 1, -1) * texel);
    float3 sw = SampleScene(uv + float2(-1,  1) * texel);
    float3 se = SampleScene(uv + float2( 1,  1) * texel);
    float3 m  = SampleScene(uv);

    float lNW = dot(nw, LUMA), lNE = dot(ne, LUMA);
    float lSW = dot(sw, LUMA), lSE = dot(se, LUMA), lM = dot(m, LUMA);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    if (lMax - lMin < lMax * 0.125f + 0.0625f)
        return m; // low local contrast: nothing to smooth

    float2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));

    float reduce = max((lNW + lNE + lSW + lSE) * 0.03125f, 0.0078125f);
    float rcpMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpMin, -1.5f, 1.5f) * texel;

    float3 rgbA = 0.5f * (SampleScene(uv + dir * (1.0f / 3.0f - 0.5f))
                        + SampleScene(uv + dir * (2.0f / 3.0f - 0.5f)));
    float3 rgbB = rgbA * 0.5f + 0.25f * (SampleScene(uv + dir * -0.5f)
                                       + SampleScene(uv + dir *  0.5f));
    float lB = dot(rgbB, LUMA);
    return (lB < lMin || lB > lMax) ? rgbA : rgbB;
}

// Cheap single-pass bloom: bright-pass a wide Vogel disk of taps, weighted by a
// falloff so the glow concentrates near its source and fades out (a real glow)
// instead of averaging into a flat haze.
float3 Bloom(float2 uv, float2 texel)
{
    const int N = 24;
    const float GOLDEN_ANGLE = 2.39996323f;

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;
    [unroll] for (int i = 0; i < N; ++i)
    {
        float t = (i + 0.5f) / N;          // 0..1 across the disk
        float r = sqrt(t) * BloomRadius;
        float a = i * GOLDEN_ANGLE;
        float3 c = SampleScene(uv + float2(cos(a), sin(a)) * r * texel);
        float bright = max(dot(c, LUMA) - BloomThreshold, 0.0f);
        float w = exp(-t * 3.5f);          // gaussian-ish falloff toward the edge
        sum += c * bright * w;
        weightSum += w;
    }
    return sum / max(weightSum, 1e-4f);
}

float4 main(PSInput input) : SV_Target0
{
    float2 texel = 1.0f / ScreenSize;

    float3 color = FXAA(input.UV, texel);
    color += Bloom(input.UV, texel) * BloomIntensity;

    return float4(color, 1.0f);
}
