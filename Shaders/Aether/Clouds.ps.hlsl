// Volumetric clouds (Nubis / Horizon Zero Dawn style). A fullscreen raymarch
// through a horizontal cloud layer. Density is built from a large-scale
// coverage/weather map (dense cumulus regions vs clear blue holes), a billowy
// base shape (bulbous cauliflower puffs) and high-frequency detail erosion,
// shaped vertically by a height gradient. Lighting uses a dual-lobe
// Henyey-Greenstein phase (forward silver lining + back light), a multi-scatter
// Beer approximation and a powder term, plus a sky-ambient gradient. The lit
// clouds are composited over a procedural sky and tonemapped.

[[vk::binding(0, 0)]]
cbuffer cbCloud : register(b0)
{
    float4x4 InvViewProj;
    float4 CameraPos;    // xyz, w = Time
    float4 SunDir;       // xyz = toward sun, w = PhaseG
    float4 SunColor;     // xyz = color, w = Intensity
    float4 SkyZenith;    // xyz = sky/ambient color, w = CloudBottom altitude
    float4 SkyHorizon;   // xyz = horizon ambient, w = CloudTop altitude
    float4 CloudParams;  // x=Coverage, y=DensityMul, z=BaseScale, w=WindSpeed
    float4 CloudParams2; // x=DetailScale, y=DetailStrength, z=Steps, w=LightSteps
    float4 CloudParams3; // x=CoverageScale, y=PowderStrength, z=AmbientStrength, w=Exposure
};

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

static const float PI = 3.14159265f;

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

// Standard fbm (smooth), used for the coverage/weather map.
float Fbm(float3 p, int octaves)
{
    float v = 0.0f, a = 0.5f, tot = 0.0f;
    [loop] for (int i = 0; i < octaves; ++i)
    {
        v += a * ValueNoise(p);
        tot += a;
        p *= 2.02f;
        a *= 0.5f;
    }
    return v / max(tot, 1e-4f);
}

// Billow fbm: abs() of signed noise makes bulbous, cauliflower-like lobes that
// read as puffy cumulus rather than smooth haze.
float BillowFbm(float3 p, int octaves)
{
    float v = 0.0f, a = 0.5f, tot = 0.0f;
    [loop] for (int i = 0; i < octaves; ++i)
    {
        v += a * abs(2.0f * ValueNoise(p) - 1.0f);
        tot += a;
        p *= 2.03f;
        a *= 0.5f;
    }
    return v / max(tot, 1e-4f);
}

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * PI * pow(max(1.0f + g2 - 2.0f * g * cosTheta, 1e-4f), 1.5f));
}

// Static white-noise dither for the raymarch start offset. Unlike Bayer (grid)
// or interleaved-gradient (diagonal streaks), white noise has no coherent
// structure, so the composite blur dissolves it into a smooth softness instead
// of a visible pattern. No time term -> it doesn't shimmer under camera motion.
float DitherOffset(float2 pix)
{
    return Hash13(float3(pix, 7.0f));
}

// Vertical density profile: flat rounded base, billowing toward the middle,
// fading near the top.
float HeightGradient(float h)
{
    float bottom = smoothstep(0.0f, 0.08f, h); // flat, full base
    float top = smoothstep(1.0f, 0.55f, h);    // fade the upper portion
    return bottom * top;
}

// Large-scale coverage/weather: 0 = clear sky, 1 = full cloud. Drives where
// clouds form so the sky has dense masses and open blue holes.
float Coverage(float3 p)
{
    float time = CameraPos.w;
    float2 uv = p.xz * CloudParams3.x + time * CloudParams.w * 0.02f;
    float w = Fbm(float3(uv, 0.0f), 3);
    // Higher threshold + sharper contrast -> distinct cumulus islands separated
    // by clear blue, rather than one connected deck.
    return saturate((w - 0.40f) * 2.2f) * CloudParams.x;
}

// Cheap base density (coverage * billow shape * height gradient). Used by the
// secondary light march.
float DensityBase(float3 p, float slabBottom, float slabTop)
{
    float h = saturate((p.y - slabBottom) / max(slabTop - slabBottom, 1e-3f));
    float grad = HeightGradient(h);
    if (grad <= 0.0f)
        return 0.0f;

    float cov = Coverage(p);
    if (cov <= 0.0f)
        return 0.0f;

    float time = CameraPos.w;
    float3 wind = float3(1.0f, 0.0f, 0.35f) * time * CloudParams.w;

    float3 sp = (p + wind) * CloudParams.z;
    // Perlin-dominant base keeps the cloud masses smooth and connected (natural
    // stratocumulus); a little billow adds puffiness. Pure billow was the source
    // of the "popcorn" look on the small gap-filling clouds.
    float perlin = Fbm(sp, 3);
    float billow = BillowFbm(sp, 2);
    float shape = lerp(perlin, billow, 0.45f);
    // Raise the cloud-forming threshold with altitude: the base forms easily
    // (wide, flat bottom) while the top needs denser shape (narrower, rounded
    // top) -- the classic cumulus profile. The softer transition also ramps
    // density gradually at the edges so the raymarch resolves it without sparkle.
    float d = saturate((shape - (1.0f - cov) - h * 0.4f) * 2.0f);
    return d * grad;
}

// Full density: base shape eroded by high-frequency billow detail. Used by the
// primary view march.
float DensityFull(float3 p, float slabBottom, float slabTop)
{
    float d = DensityBase(p, slabBottom, slabTop);
    if (d <= 0.0f)
        return 0.0f;

    float time = CameraPos.w;
    float3 wind = float3(1.0f, 0.0f, 0.35f) * time * CloudParams.w;
    // High-frequency Perlin detail erodes the edges into fine wispy tendrils so
    // the cloud reads as a gaseous body rather than a smooth solid/liquid blob.
    // (Perlin, not billow, keeps it wispy instead of spiky.)
    float detail = Fbm((p + wind * 1.6f) * CloudParams2.x, 4);
    d = saturate((d - detail * CloudParams2.y * (1.0f - d)));

    return d * CloudParams.y;
}

// Optical depth toward the sun (short march of the cheap base density).
float LightMarch(float3 p, float slabBottom, float slabTop)
{
    int lightSteps = (int)CloudParams2.w;
    float3 L = normalize(SunDir.xyz);
    float stepLen = (slabTop - slabBottom) / 6.0f;

    float depth = 0.0f;
    float3 sp = p;
    [loop] for (int i = 0; i < lightSteps; ++i)
    {
        sp += L * stepLen;
        depth += DensityBase(sp, slabBottom, slabTop) * CloudParams.y * stepLen;
    }
    return depth;
}

// Outputs the lit cloud (premultiplied scatter, rgb) and the background
// transmittance (a) at half resolution. A later full-res pass composites this
// over the sharp scene + sky: finalColor = background * a + scatter.
float4 main(PSInput input) : SV_Target0
{
    // Reconstruct the world-space view ray for this pixel.
    float2 ndc = input.UV * 2.0f - 1.0f;
    float4 farPoint = mul(InvViewProj, float4(ndc, 1.0f, 1.0f));
    float3 worldFar = farPoint.xyz / farPoint.w;
    float3 ro = CameraPos.xyz;
    float3 rd = normalize(worldFar - ro);

    float slabBottom = SkyZenith.w;
    float slabTop = SkyHorizon.w;

    float3 L = normalize(SunDir.xyz);
    float cosA = dot(rd, L);
    // Dual-lobe phase: a strong forward lobe (silver lining toward the sun) plus
    // a weaker back lobe so clouds facing away still catch some light.
    float phase = max(HenyeyGreenstein(cosA, SunDir.w), 0.7f * HenyeyGreenstein(cosA, -0.15f));

    float transmittance = 1.0f;
    float3 scatter = 0.0f;

    // Skip the march entirely for rays that never cross the slab.
    if (abs(rd.y) >= 1e-4f)
    {
        float t0 = (slabBottom - ro.y) / rd.y;
        float t1 = (slabTop - ro.y) / rd.y;
        float tEnter = max(min(t0, t1), 0.0f);
        float tExit = max(t0, t1);

        if (tExit > tEnter)
        {
            // March far enough that grazing rays keep hitting cloud all the way
            // to the horizon (aerial perspective below fades them into haze).
            const float maxDist = 1600.0f;
            tExit = min(tExit, tEnter + maxDist);

            int steps = (int)CloudParams2.z;
            float stepLen = (tExit - tEnter) / (float)steps;

            float jitter = DitherOffset(input.Pos.xy);
            float t = tEnter + stepLen * jitter;

            [loop] for (int i = 0; i < steps; ++i)
            {
                float3 p = ro + rd * t;
                float density = DensityFull(p, slabBottom, slabTop);
                // Only fade the very farthest slice so there's no hard cutoff,
                // while keeping the cloud deck solid all the way to the horizon.
                density *= saturate((maxDist - t) / (0.18f * maxDist));

                if (density > 0.01f)
                {
                    float lightDepth = LightMarch(p, slabBottom, slabTop);

                    // Multi-scatter Beer: three octaves broaden the response, but
                    // with a lower floor than before so sun-facing bulges stay
                    // bright while shadowed crevices/bases go genuinely dark --
                    // that contrast is what reads as cloud volume.
                    float ms = exp(-lightDepth)
                             + 0.45f * exp(-0.45f * lightDepth)
                             + 0.22f * exp(-0.12f * lightDepth);
                    float powder = 1.0f - exp(-lightDepth * 2.0f);
                    powder = lerp(1.0f, powder, CloudParams3.y);

                    float3 sun = SunColor.xyz * SunColor.w * ms * phase * powder;

                    // Ambient sky fill: darker bluish base -> brighter top.
                    float h = saturate((p.y - slabBottom) / max(slabTop - slabBottom, 1e-3f));
                    // Skylight comes from the blue sky dome, so sun-shadowed
                    // undersides pick up a blue tint (not grey). Bases a touch
                    // darker, tops brighter; the deepest self-shadowed pockets
                    // keep real darkness via a gentle ambient occlusion.
                    float3 skyAmbient = lerp(SkyHorizon.xyz, SkyZenith.xyz, 0.7f);
                    float3 ambient = skyAmbient * lerp(0.75f, 1.15f, h) * CloudParams3.z;
                    ambient *= lerp(0.74f, 1.0f, saturate(ms));

                    float3 srcCol = sun + ambient;

                    // Aerial perspective: only the far horizon band fades to
                    // haze. Near/mid clouds keep their full lit form (starting
                    // the fade well past the camera avoids flattening them).
                    float aerial = 1.0f - exp(-max(t - 400.0f, 0.0f) * 0.0011f);
                    srcCol = lerp(srcCol, SkyHorizon.xyz, aerial);

                    float dt = density * stepLen;
                    float tr = exp(-dt);
                    scatter += transmittance * srcCol * (1.0f - tr);
                    transmittance *= tr;
                    if (transmittance < 0.01f)
                        break;
                }
                t += stepLen;
            }
        }
    }

    // Tonemap the scatter so the bright silver lining rolls off instead of
    // clipping in the 8-bit half-res target; the composite adds it to the scene.
    scatter = 1.0f - exp(-scatter * CloudParams3.w);

    return float4(scatter, transmittance);
}
