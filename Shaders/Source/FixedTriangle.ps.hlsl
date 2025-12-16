[[vk::binding(0, 0)]]
Texture2D texture : register(t0);

[[vk::binding(1, 0)]]
SamplerState sampl : register(s0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 Tex : TEXCOORD;
};

float4 main(PSInput input) : SV_Target0
{
    return texture.Sample(sampl, input.Tex);
}