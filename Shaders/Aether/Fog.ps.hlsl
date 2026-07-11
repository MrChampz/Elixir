// Froxel fog apply pass. Samples the integrated froxel grid at this pixel's
// column (bilinear in XY, full-depth slice) and composites the fog over the
// scene: finalColor = scene * transmittance + inScattering.

[[vk::binding(0, 0)]]
cbuffer cbFroxel : register(b0)
{
    float4x4 InvViewProj;
    float4 CameraPos;
    float4 FogAlbedo;
    float4 BoxCenter;
    float4 BoxHalfExtents;
    float4 SphereCenter;
    float4 LightDir;
    float4 LightColor;
    float4 GridSize; // x=W, y=H, z=D
};

[[vk::binding(1, 0)]]
StructuredBuffer<float4> froxels;

[[vk::binding(0, 1)]]
Texture2D sceneColor : register(t0);

[[vk::binding(1, 1)]]
SamplerState fogSampler : register(s0);

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

float4 SampleFroxel(int x, int y, int z, int W, int H, int D)
{
    x = clamp(x, 0, W - 1);
    y = clamp(y, 0, H - 1);
    z = clamp(z, 0, D - 1);
    return froxels[x + y * W + z * W * H];
}

float4 main(PSInput input) : SV_Target0
{
    int W = (int)GridSize.x;
    int H = (int)GridSize.y;
    int D = (int)GridSize.z;

    // No scene depth yet: sample the deepest slice (full integration).
    int z = D - 1;

    float2 g = input.UV * float2((float)W, (float)H) - 0.5f;
    int2 gi = (int2)floor(g);
    float2 gf = frac(g);

    float4 f00 = SampleFroxel(gi.x,     gi.y,     z, W, H, D);
    float4 f10 = SampleFroxel(gi.x + 1, gi.y,     z, W, H, D);
    float4 f01 = SampleFroxel(gi.x,     gi.y + 1, z, W, H, D);
    float4 f11 = SampleFroxel(gi.x + 1, gi.y + 1, z, W, H, D);
    float4 fog = lerp(lerp(f00, f10, gf.x), lerp(f01, f11, gf.x), gf.y);

    // Extended Reinhard tonemap on the in-scattering: backlit fog accumulates
    // large radiance along the ray, which clips to flat white in LDR. This rolls
    // the highlights off toward white while leaving mid values (thin/side-lit
    // fog) essentially unchanged.
    float3 scatter = fog.rgb;
    const float white = 2.5f;
    scatter = scatter * (1.0f + scatter / (white * white)) / (1.0f + scatter);

    float3 scene = sceneColor.Sample(fogSampler, input.UV).rgb;
    float3 finalColor = scene * fog.a + scatter;

    return float4(finalColor, 1.0f);
}
