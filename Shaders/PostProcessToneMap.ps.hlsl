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
Texture2D bloomTarget : register(t1);

[[vk::binding(2, 1)]]
SamplerState postProcessSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

float SanitizeHDRChannel(const float value)
{
    static const float maxHDRValue = 65504.0f;
    if (value >= 0.0f && value <= maxHDRValue)
        return value;

    return value > maxHDRValue ? maxHDRValue : 0.0f;
}

float3 ACESFilm(const float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 main(PSInput input) : SV_Target0
{
    const float3 sampledScene = sceneTarget.Sample(postProcessSampler, input.UV).rgb;
    const float3 sampledBloom = bloomTarget.Sample(postProcessSampler, input.UV).rgb;
    const float3 scene = float3(
        SanitizeHDRChannel(sampledScene.r),
        SanitizeHDRChannel(sampledScene.g),
        SanitizeHDRChannel(sampledScene.b)
    );
    const float3 bloom = float3(
        SanitizeHDRChannel(sampledBloom.r),
        SanitizeHDRChannel(sampledBloom.g),
        SanitizeHDRChannel(sampledBloom.b)
    );
    const float3 hdrColor = (scene + bloom * BloomIntensity) * exp2(Exposure);
    return float4(ACESFilm(hdrColor), 1.0f);
}
