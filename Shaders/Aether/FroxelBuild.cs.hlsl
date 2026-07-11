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

// Fog density at a world position (bounded volumes modulated by contrast noise).
float SampleDensity(float3 p)
{
    float mask = max(BoxMask(p), SphereMask(p));
    if (mask <= 0.0f)
        return 0.0f;

    float3 drift = float3(0.0f, -SphereCenter.w * 0.04f, SphereCenter.w * 0.02f);
    float f = Fbm(p * LightDir.w + drift);           // LightDir.w = NoiseScale
    f = saturate((f - 0.42f) * 2.6f + 0.5f);
    float noise = lerp(1.0f, f, LightColor.w);        // LightColor.w = NoiseStrength
    return FogAlbedo.w * mask * max(noise, 0.0f);     // FogAlbedo.w = Density
}

// Volumetric shadow: march toward a light through the fog and return how much
// light survives (self-shadowing -> god-ray shafts).
float LightTransmittance(float3 p, float3 dir, float marchDist, int steps)
{
    float stepLen = marchDist / (float)steps;
    float accum = 0.0f;
    float3 sp = p + dir * (stepLen * 0.5f);
    [loop] for (int i = 0; i < steps; ++i)
    {
        accum += SampleDensity(sp) * stepLen;
        sp += dir * stepLen;
    }
    return exp(-accum);
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

    const int SHADOW_STEPS = 6;
    const float SHADOW_DIST = 8.0f;

    float maxDist = CameraPos.w;
    float aniso = GridSize.w;

    // View ray for this column (constant across depth slices).
    float2 ndc = ((float2(id.xy) + 0.5f) / float2((float)W, (float)H)) * 2.0f - 1.0f;
    float4 farPoint = mul(InvViewProj, float4(ndc, 1.0f, 1.0f));
    float3 worldFar = farPoint.xyz / farPoint.w;
    float3 rayDir = normalize(worldFar - CameraPos.xyz);

    // Directional light is constant along the column (ray direction is fixed).
    float3 sunDir = normalize(LightDir.xyz);
    float dirPhase = HenyeyGreenstein(dot(rayDir, sunDir), aniso);
    float3 dirLight = LightColor.xyz * LightParams.x * dirPhase;
    int pointCount = (int)LightParams.w;

    // Basis perpendicular to the sun, for projecting a shaft "gobo" that is
    // constant along the light direction (-> parallel beams) and varies across.
    float3 sunUp = abs(sunDir.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 sunRight = normalize(cross(sunDir, sunUp));
    float3 sunFwd = cross(sunRight, sunDir);

    float stepDepth = maxDist / (float)D;
    float transmittance = 1.0f;
    float3 scatterAccum = 0.0f;

    for (uint z = 0; z < D; ++z)
    {
        float dist = ((float)z + 0.5f) * stepDepth;
        float3 p = CameraPos.xyz + rayDir * dist;

        float density = SampleDensity(p);

        // Directional god-ray shafts: a gobo pattern projected along the sun
        // carves the light into parallel beams (constant along sunDir, varying
        // across it), combined with the fog's own self-shadow. Ambient is not
        // shadowed.
        // A single bounded window in the plane perpendicular to the sun: a small
        // rectangle divided into a 2x2 grid of panes by mullions. Everything
        // outside it is "wall" (no light). Constant along the sun direction, so
        // the panes cast clean rectangular shafts -- sunlight through a window.
        float2 pw = float2(dot(p, sunRight), dot(p, sunFwd));
        float2 winCenter = float2(0.0f, 1.0f);
        float2 winHalf = float2(2.0f, 2.6f);
        float2 local = (pw - winCenter) / winHalf; // -1..1 inside the window

        float pane = 0.0f;
        if (all(abs(local) < 1.0f))
        {
            float2 cell = frac((local * 0.5f + 0.5f) * 2.0f); // 2x2 panes
            float2 barDist = min(cell, 1.0f - cell);
            // Wide, soft transitions so the pane/mullion edges are gradients the
            // froxel grid can resolve without stair-stepping (anti-aliasing).
            float mull = 0.10f, soft = 0.18f;
            pane = smoothstep(mull, mull + soft, barDist.x) * smoothstep(mull, mull + soft, barDist.y);
            // soft outer frame so the window edge is not a hard cut
            float2 edge = 1.0f - abs(local);
            pane *= smoothstep(0.0f, 0.35f, min(edge.x, edge.y));
        }

        float dirShadow = LightTransmittance(p, sunDir, SHADOW_DIST, SHADOW_STEPS);

        float3 light = LightParams.y;
        light += dirLight * dirShadow * (0.05f + 2.4f * pane);

        if (pointCount > 0)
        {
            float3 toL = PointLight0PosRange.xyz - p;
            float dL = length(toL);
            float shadow = LightTransmittance(p, toL / max(dL, 1e-4f), min(dL, SHADOW_DIST), SHADOW_STEPS);
            light += PointLight(p, rayDir, PointLight0PosRange, PointLight0Color, aniso) * shadow;
        }
        if (pointCount > 1)
        {
            float3 toL = PointLight1PosRange.xyz - p;
            float dL = length(toL);
            float shadow = LightTransmittance(p, toL / max(dL, 1e-4f), min(dL, SHADOW_DIST), SHADOW_STEPS);
            light += PointLight(p, rayDir, PointLight1PosRange, PointLight1Color, aniso) * shadow;
        }

        float3 inScatter = FogAlbedo.xyz * light * (density * LightParams.z);

        float t = exp(-density * stepDepth);
        scatterAccum += transmittance * inScatter * stepDepth;
        transmittance *= t;

        uint idx = id.x + id.y * W + z * W * H;
        froxels[idx] = float4(scatterAccum, transmittance);
    }
}
