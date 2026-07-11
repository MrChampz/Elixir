// Heat-haze refraction: offset the scene-color lookup by a scrolling normal map
// so the billboard bends whatever was rendered behind it.

// Bindless set (shared with the sprite renderer) holds the per-emitter
// distortion normal map, indexed by the push constant.
[[vk::binding(1, 1)]]
Texture2D sprites[] : register(t0);

[[vk::binding(1, 0)]]
SamplerState spriteSampler : register(s0);

// Snapshot of the scene taken before this pass (bound once, same object each
// frame).
[[vk::binding(2, 0)]]
Texture2D sceneColor : register(t1);

struct DistortPushConstants
{
    uint DistortIndex;
    float Strength;
    float ScrollX;
    float ScrollY;
    float ViewW;
    float ViewH;
};
[[vk::push_constant]]
DistortPushConstants pc;

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float4 Color   : COLOR;
    float2 QuadUV  : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    // Scrolling distortion normal -> screen-space UV offset.
    float2 duv = input.QuadUV + float2(pc.ScrollX, pc.ScrollY);
    float2 n = sprites[pc.DistortIndex].Sample(spriteSampler, duv).xy * 2.0f - 1.0f;
    float2 offset = n * pc.Strength;

    // The fragment's own screen position gives the base scene-color UV.
    float2 screenUV = input.ClipPos.xy / float2(pc.ViewW, pc.ViewH);
    float2 refractedUV = clamp(screenUV + offset, 0.0f, 1.0f);

    float3 refracted = sceneColor.Sample(spriteSampler, refractedUV).rgb;

    // Feather by the particle alpha and a soft radial falloff so the quad edges
    // do not show a hard rectangle.
    float2 c = input.QuadUV - 0.5f;
    float radial = saturate(1.0f - dot(c, c) * 4.0f);
    float mask = input.Color.a * radial;

    return float4(refracted, mask);
}
