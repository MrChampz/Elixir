// Cloud composite (full resolution). Samples the half-res cloud result
// (scatter.rgb, transmittance.a, already upscaled with a linear blit), builds a
// sharp procedural sky behind it and composites over the scene:
//   background = lerp(scene, sky, skyMask)   // sky only in the empty background
//   finalColor = background * transmittance + scatter

[[vk::binding(0, 0)]]
cbuffer cbCloud : register(b0)
{
    float4x4 InvViewProj;
    float4 CameraPos;    // xyz, w = Time
    float4 SunDir;       // xyz = toward sun, w = PhaseG
    float4 SunColor;     // xyz = color, w = Intensity
    float4 SkyZenith;    // xyz = sky/ambient color, w = CloudBottom altitude
    float4 SkyHorizon;   // xyz = horizon ambient, w = CloudTop altitude
    float4 CloudParams;
    float4 CloudParams2;
    float4 CloudParams3;
};

[[vk::binding(0, 1)]]
Texture2D sceneColor : register(t0);

[[vk::binding(1, 1)]]
Texture2D cloudTex : register(t1);

[[vk::binding(2, 1)]]
SamplerState linearSampler : register(s0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

// Procedural sky dome: a full sphere. Upper hemisphere fades paler horizon ->
// deeper zenith; lower hemisphere continues the gradient down to a darker
// "ground" tone so the sky wraps all the way around, with a warm sun glow.
float3 SkyGradient(float3 rd)
{
    float3 ground = SkyHorizon.xyz * 0.35f;
    float3 sky;
    if (rd.y >= 0.0f)
        sky = lerp(SkyHorizon.xyz, SkyZenith.xyz, pow(saturate(rd.y), 0.5f));
    else
        sky = lerp(SkyHorizon.xyz, ground, pow(saturate(-rd.y), 0.6f));

    float sun = pow(saturate(dot(rd, normalize(SunDir.xyz))), 8.0f);
    sky += SunColor.xyz * sun * 0.4f;
    return sky;
}

float4 main(PSInput input) : SV_Target0
{
    float3 scene = sceneColor.Sample(linearSampler, input.UV).rgb;

    // 3x3 tent blur of the clouds only (not the scene/sky) to soften the
    // half-res raymarch grain, heaviest on the wispy low-density fringes where
    // it lives. fwidth gives one full-res texel in UV, so the kernel spans a
    // couple of half-res texels regardless of resolution.
    float2 o = fwidth(input.UV) * 2.2f;
    float4 cloud = cloudTex.Sample(linearSampler, input.UV) * 0.25f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2( o.x, 0.0f)) * 0.125f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2(-o.x, 0.0f)) * 0.125f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2(0.0f,  o.y)) * 0.125f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2(0.0f, -o.y)) * 0.125f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2( o.x,  o.y)) * 0.0625f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2(-o.x,  o.y)) * 0.0625f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2( o.x, -o.y)) * 0.0625f;
    cloud += cloudTex.Sample(linearSampler, input.UV + float2(-o.x, -o.y)) * 0.0625f;

    float3 scatter = cloud.rgb;
    float transmittance = cloud.a;

    // Reconstruct the world-space view ray for the sky.
    float2 ndc = input.UV * 2.0f - 1.0f;
    float4 farPoint = mul(InvViewProj, float4(ndc, 1.0f, 1.0f));
    float3 worldFar = farPoint.xyz / farPoint.w;
    float3 rd = normalize(worldFar - CameraPos.xyz);

    // Sky dome fills the whole empty background (both hemispheres), only where
    // the scene is dark (no geometry) so foreground objects survive.
    float sceneLum = dot(scene, float3(0.299f, 0.587f, 0.114f));
    float skyMask = 1.0f - smoothstep(0.015f, 0.15f, sceneLum);
    float3 background = lerp(scene, SkyGradient(rd), skyMask);

    float3 col = background * transmittance + scatter;
    return float4(col, 1.0f);
}
