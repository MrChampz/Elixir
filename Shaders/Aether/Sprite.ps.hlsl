// Global bindless resources (binding 0 = cis, binding 1 = textures, binding 2 = samplers)
[[vk::binding(1, 1)]]
Texture2D sprites[] : register(t0);

[[vk::binding(1, 0)]]
SamplerState spriteSampler : register(s0);

struct SpritePushConstants
{
    uint SpriteIndex;
};
[[vk::push_constant]]
SpritePushConstants pc;

struct PSInput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;
    float2 TexCoord      : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    float4 sprite = sprites[pc.SpriteIndex].Sample(spriteSampler, input.TexCoord);

    float3 color = input.Color.rgb * sprite.rgb;
    float alpha = input.Color.a * sprite.a;

    return float4(color, alpha);
}