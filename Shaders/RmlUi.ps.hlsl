[[vk::binding(1, 1)]]
Texture2D textures[] : register(t0);

[[vk::binding(1, 0)]]
SamplerState textureSampler : register(s0);

struct PS_INPUT
{
    float4 ClipPosition : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    nointerpolation uint TextureIndex : TEXTURE;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return textures[input.TextureIndex].Sample(textureSampler, input.TexCoord) * input.Color;
}
