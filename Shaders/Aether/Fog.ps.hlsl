// Volumetric fog (phase 1): reconstruct a world-space view ray per pixel and
// raymarch a height-based fog medium modulated by animated 3D noise, then
// composite the accumulated fog over the scene by alpha blending. No depth yet
// (fills the view volume up to MaxDistance); depth-awareness comes next.

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
    float4x4 InvViewProj;
};

struct FogPushConstants
{
    float3 FogColor;
    float Density;
    float3 BoxCenter;
    float SphereRadius;
    float3 BoxHalfExtents;
    float EdgeFeather;   // soft-edge width of the volumes
    float3 SphereCenter;
    float Time;
    float MaxDistance;   // ray march reach
    float NoiseScale;
    float NoiseStrength; // 0 = uniform, 1 = fully noise-modulated
    float StepCount;
};
[[vk::push_constant]]
FogPushConstants pc;

// Soft box mask: 1 inside, feathering to 0 within EdgeFeather of the surface.
float BoxMask(float3 p)
{
    float3 q = abs(p - pc.BoxCenter) - pc.BoxHalfExtents;
    float outside = length(max(q, 0.0f));
    return 1.0f - smoothstep(0.0f, pc.EdgeFeather, outside);
}

// Soft sphere mask.
float SphereMask(float3 p)
{
    float d = distance(p, pc.SphereCenter);
    return 1.0f - smoothstep(pc.SphereRadius - pc.EdgeFeather, pc.SphereRadius, d);
}

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

float Hash13(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float n000 = Hash13(i + float3(0, 0, 0));
    float n100 = Hash13(i + float3(1, 0, 0));
    float n010 = Hash13(i + float3(0, 1, 0));
    float n110 = Hash13(i + float3(1, 1, 0));
    float n001 = Hash13(i + float3(0, 0, 1));
    float n101 = Hash13(i + float3(1, 0, 1));
    float n011 = Hash13(i + float3(0, 1, 1));
    float n111 = Hash13(i + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

float Fbm(float3 p)
{
    float v = 0.0f;
    float a = 0.5f;
    [unroll] for (int i = 0; i < 3; ++i)
    {
        v += a * ValueNoise(p);
        p *= 2.03f;
        a *= 0.5f;
    }
    return v;
}

float4 main(PSInput input) : SV_Target0
{
    // Reconstruct the world-space ray through this pixel from the far plane.
    float2 ndc = input.UV * 2.0f - 1.0f;
    float4 farPoint = mul(InvViewProj, float4(ndc, 1.0f, 1.0f));
    float3 worldFar = farPoint.xyz / farPoint.w;
    float3 rayDir = normalize(worldFar - CameraPos);

    int steps = max((int)pc.StepCount, 1);
    float stepLen = pc.MaxDistance / (float)steps;

    float3 pos = CameraPos;
    float3 drift = float3(0.0f, -pc.Time * 0.04f, pc.Time * 0.02f);

    float transmittance = 1.0f;
    [loop] for (int s = 0; s < steps; ++s)
    {
        pos += rayDir * stepLen;

        // Confine the fog to the union of the box and sphere volumes.
        float mask = max(BoxMask(pos), SphereMask(pos));
        if (mask <= 0.0f)
            continue;

        float noise = Fbm(pos * pc.NoiseScale + drift);
        noise = lerp(1.0f, noise, pc.NoiseStrength);

        float density = pc.Density * mask * max(noise, 0.0f);
        transmittance *= exp(-density * stepLen);
    }

    float fog = saturate(1.0f - transmittance);
    return float4(pc.FogColor, fog);
}
