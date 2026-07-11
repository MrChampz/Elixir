// Froxel volumetric fog build pass. One thread per (x,y) grid column marches
// through the depth slices, evaluating the fog medium (bounded box/sphere
// volumes modulated by 3D noise), computing in-scattering from a directional
// light (Henyey-Greenstein phase), and integrating front-to-back. Each froxel
// stores the accumulated (scattering.rgb, transmittance) from the camera up to
// its slice, so the apply pass is a single lookup.

[[vk::binding(0, 0)]]
RWStructuredBuffer<float4> froxels;

[[vk::binding(0, 1)]]
cbuffer cbFroxel : register(b0)
{
    float4x4 InvViewProj;
    float4 CameraPos;      // xyz, w = MaxDistance
    float4 FogAlbedo;      // xyz, w = Density
    float4 BoxCenter;      // xyz, w = SphereRadius
    float4 BoxHalfExtents; // xyz, w = EdgeFeather
    float4 SphereCenter;   // xyz, w = Time
    float4 LightDir;       // xyz, w = NoiseScale
    float4 LightColor;     // xyz, w = NoiseStrength
    float4 GridSize;       // x=W, y=H, z=D, w = Anisotropy
    float4 LightParams;    // x=DirIntensity, y=Ambient, z=ScatterStrength, w=PointCount
    float4 PointLight0PosRange;
    float4 PointLight0Color; // w = intensity
    float4 PointLight1PosRange;
    float4 PointLight1Color;
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
    return lerp(lerp(nx00, nx10, f.y), lerp(nx01, nx11, f.y), f.z);
}

float Fbm(float3 p)
{
    float v = 0.0f, a = 0.5f;
    [unroll] for (int i = 0; i < 3; ++i) { v += a * ValueNoise(p); p *= 2.03f; a *= 0.5f; }
    return v;
}

float BoxMask(float3 p)
{
    float3 q = abs(p - BoxCenter.xyz) - BoxHalfExtents.xyz;
    float outside = length(max(q, 0.0f));
    return 1.0f - smoothstep(0.0f, BoxHalfExtents.w, outside);
}

float SphereMask(float3 p)
{
    float d = distance(p, SphereCenter.xyz);
    return 1.0f - smoothstep(BoxCenter.w - BoxHalfExtents.w, BoxCenter.w, d);
}

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * 3.14159265f * pow(max(1.0f + g2 - 2.0f * g * cosTheta, 1e-4f), 1.5f));
}

// In-scattered radiance from a point light at a froxel sample.
float3 PointLight(float3 p, float3 rayDir, float4 posRange, float4 colorInt, float aniso)
{
    float3 toLight = posRange.xyz - p;
    float dist = length(toLight);
    float3 L = toLight / max(dist, 1e-4f);
    float atten = saturate(1.0f - dist / max(posRange.w, 1e-4f));
    atten *= atten;
    return colorInt.xyz * colorInt.w * atten * HenyeyGreenstein(dot(rayDir, L), aniso);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W = (uint)GridSize.x;
    uint H = (uint)GridSize.y;
    uint D = (uint)GridSize.z;
    if (id.x >= W || id.y >= H)
        return;

    float maxDist = CameraPos.w;
    float density0 = FogAlbedo.w;
    float noiseScale = LightDir.w;
    float noiseStrength = LightColor.w;
    float aniso = GridSize.w;

    // View ray for this column (constant across depth slices).
    float2 ndc = ((float2(id.xy) + 0.5f) / float2((float)W, (float)H)) * 2.0f - 1.0f;
    float4 farPoint = mul(InvViewProj, float4(ndc, 1.0f, 1.0f));
    float3 worldFar = farPoint.xyz / farPoint.w;
    float3 rayDir = normalize(worldFar - CameraPos.xyz);

    // Directional light is constant along the column (ray direction is fixed).
    float dirPhase = HenyeyGreenstein(dot(rayDir, normalize(LightDir.xyz)), aniso);
    float3 dirLight = LightColor.xyz * LightParams.x * dirPhase;
    int pointCount = (int)LightParams.w;
    float3 drift = float3(0.0f, -SphereCenter.w * 0.04f, SphereCenter.w * 0.02f);

    float stepDepth = maxDist / (float)D;
    float transmittance = 1.0f;
    float3 scatterAccum = 0.0f;

    for (uint z = 0; z < D; ++z)
    {
        float dist = ((float)z + 0.5f) * stepDepth;
        float3 p = CameraPos.xyz + rayDir * dist;

        float mask = max(BoxMask(p), SphereMask(p));
        // Boost contrast so the fbm reads as defined wisps/holes rather than a
        // smooth haze (the depth integration otherwise averages fine noise out).
        float f = Fbm(p * noiseScale + drift);
        f = saturate((f - 0.42f) * 2.6f + 0.5f);
        float noise = lerp(1.0f, f, noiseStrength);
        float density = density0 * mask * max(noise, 0.0f);

        // Gather lighting: directional + ambient + point lights.
        float3 light = dirLight + LightParams.y;
        if (pointCount > 0) light += PointLight(p, rayDir, PointLight0PosRange, PointLight0Color, aniso);
        if (pointCount > 1) light += PointLight(p, rayDir, PointLight1PosRange, PointLight1Color, aniso);

        float3 inScatter = FogAlbedo.xyz * light * (density * LightParams.z);

        float t = exp(-density * stepDepth);
        scatterAccum += transmittance * inScatter * stepDepth;
        transmittance *= t;

        uint idx = id.x + id.y * W + z * W * H;
        froxels[idx] = float4(scatterAccum, transmittance);
    }
}
