[[vk::binding(0, 0)]]
cbuffer cbPostProcess : register(b0)
{
    float2 InverseSceneSize;
    float BloomThreshold;
    float BloomKnee;
    float BloomRadius;
    float BloomIntensity;
    float Exposure;
    float Padding;
};

[[vk::binding(0, 1)]]
Texture2D sceneTarget : register(t0);

[[vk::binding(1, 1)]]
SamplerState postProcessSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

float3 ExtractBloom(const float3 color)
{
    const float brightness = max(color.r, max(color.g, color.b));
    const float knee = max(BloomKnee, 1e-4f);
    const float soft = saturate((brightness - BloomThreshold + knee) / (2.0f * knee));
    const float softContribution = soft * soft * knee;
    const float contribution = max(brightness - BloomThreshold, softContribution);
    return color * contribution / max(brightness, 1e-4f);
}

float4 main(PSInput input) : SV_Target0
{
    static const float2 offsets[] = {
        float2(-1.0f, -1.0f), float2(0.0f, -1.0f), float2(1.0f, -1.0f),
        float2(-1.0f,  0.0f), float2(0.0f,  0.0f), float2(1.0f,  0.0f),
        float2(-1.0f,  1.0f), float2(0.0f,  1.0f), float2(1.0f,  1.0f),
    };
    static const float weights[] = {
        1.0f, 2.0f, 1.0f,
        2.0f, 4.0f, 2.0f,
        1.0f, 2.0f, 1.0f,
    };

    const float2 radius = InverseSceneSize * BloomRadius;
    float3 bloom = 0.0f.xxx;

    [unroll]
    for (uint sampleIndex = 0; sampleIndex < 9; ++sampleIndex)
    {
        const float3 sampleColor = sceneTarget.Sample(
            postProcessSampler,
            saturate(input.UV + offsets[sampleIndex] * radius)
        ).rgb;
        bloom += ExtractBloom(sampleColor) * weights[sampleIndex];
    }

    return float4(bloom / 16.0f, 1.0f);
}
