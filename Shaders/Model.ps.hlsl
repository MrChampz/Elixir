// glTF mesh pixel shader. Samples the base-color texture (bindless) modulated by
// the material's base-color factor, and applies a simple directional light plus
// ambient for readable shading. PBR (metallic-roughness, normal maps) comes next.

[[vk::binding(1, 0)]]
SamplerState texSampler : register(s0);

[[vk::binding(1, 1)]]
Texture2D textures[] : register(t0);

struct ModelPushConstants
{
    float4x4 Model;
    float4 BaseColorFactor;
    uint TextureIndex;
};
[[vk::push_constant]]
ModelPushConstants pc;

struct PSInput
{
    float4 ClipPos  : SV_POSITION;
    float3 Normal   : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : POSITION0;
};

float4 main(PSInput input) : SV_Target0
{
    float4 baseColor = pc.BaseColorFactor;
    if (pc.TextureIndex != 0xffffffffu)
        baseColor *= textures[pc.TextureIndex].Sample(texSampler, input.TexCoord);

    float3 N = normalize(input.Normal);
    float3 L = normalize(float3(0.4f, 0.8f, 0.35f)); // key light from upper-front
    float ndl = saturate(dot(N, L));

    float3 ambient = float3(0.28f, 0.30f, 0.34f);
    float3 lightColor = float3(1.0f, 0.97f, 0.90f);
    float3 lit = baseColor.rgb * (ambient + ndl * lightColor);

    return float4(lit, baseColor.a);
}
