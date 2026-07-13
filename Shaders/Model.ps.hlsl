// glTF metallic-roughness PBR base pass. Cook-Torrance GGX BRDF with a single
// directional light plus a flat ambient term. Normal mapping uses a
// derivative-based cotangent frame (no TANGENT attribute required). IBL comes
// later; for now metals reflect the constant ambient.

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
    uint EnvIndex;
    uint IrradianceIndex;
    float EnvIntensity;
    float EnvMaxLod; // highest mip level of the environment map
    uint PrefIndex;         // GGX-prefiltered specular atlas
    uint SceneColorIndex;   // grabbed scene colour for refraction
    float ScreenWidth;
    float ScreenHeight;
    float4 LightDirection;  // xyz = direction TO the light
    float4 LightColor;      // rgb = color, w = intensity
};

[[vk::binding(1, 0)]]
SamplerState texSampler : register(s0);

struct GPUMaterial
{
    float4 BaseColorFactor;
    float4 EmissiveMetallic;     // xyz = emissive, w = metallic
    float4 RoughOccNormalCutoff; // x = roughness, y = occlusion, z = normalScale, w = alphaCutoff
    uint4 TexIndex0;             // baseColor, metallicRoughness, normal, emissive
    uint4 TexIndex1;             // occlusion, alphaMode, unused...
    float4 BaseColorTransform;   // KHR_texture_transform: uv scale.xy, offset.zw
    float4 MetallicRoughnessTransform;
    float4 NormalTransform;
    float4 EmissiveTransform;
    float4 OcclusionTransform;
    float4 Clearcoat;           // x = factor, y = roughness
    float4 Specular;            // rgb = specular color factor, w = specular factor
};

[[vk::binding(2, 0)]]
StructuredBuffer<GPUMaterial> materials;

[[vk::binding(1, 1)]]
Texture2D textures[] : register(t0);

struct ModelPushConstants
{
    float4x4 Model;
    uint MaterialIndex;
};
[[vk::push_constant]]
ModelPushConstants pc;

struct PSInput
{
    float4 ClipPos   : SV_POSITION;
    float3 Normal    : NORMAL0;
    float4 Tangent   : TANGENT0;
    float2 TexCoord  : TEXCOORD0;
    float3 WorldPos  : POSITION0;
    bool FrontFace   : SV_IsFrontFace;
};

static const float PI = 3.14159265359f;
static const uint NO_TEXTURE = 0xffffffffu;

float2 XformUV(float2 uv, float4 t)
{
    return uv * t.xy + t.zw;
}

float3 SampleTex(uint index, float2 uv)
{
    return textures[index].Sample(texSampler, uv).rgb;
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-7f);
}

float GeometrySchlickGGX(float NdotX, float k)
{
    return NdotX / (NdotX * (1.0f - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return GeometrySchlickGGX(NdotV, k) * GeometrySchlickGGX(NdotL, k);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Equirectangular direction -> UV (matches the CPU irradiance bake).
float2 DirToEquirect(float3 d)
{
    float u = atan2(d.z, d.x) * 0.15915494f + 0.5f;
    float v = acos(clamp(d.y, -1.0f, 1.0f)) * 0.31830989f;
    return float2(u, v);
}

float3 SampleEnvLod(float3 dir, float lod)
{
    if (EnvIndex == NO_TEXTURE)
        return float3(0.1f, 0.12f, 0.15f);
    float3 c = textures[EnvIndex].SampleLevel(texSampler, DirToEquirect(dir), lod).rgb;
    // Firefly clamp: the HDR's bright light sources, scattered by the fine flake
    // normal, otherwise blow out into sparkle. Capping keeps highlights bright
    // without extreme spikes.
    c = min(c, 3.0f);
    return c * EnvIntensity;
}

float3 SampleIrradiance(float3 dir)
{
    if (IrradianceIndex == NO_TEXTURE)
        return float3(0.1f, 0.12f, 0.15f);
    return textures[IrradianceIndex].SampleLevel(texSampler, DirToEquirect(dir), 0).rgb * EnvIntensity;
}

// GGX-prefiltered specular atlas: PREF_LEVELS equirect blocks stacked vertically,
// block k convolved for roughness k/(PREF_LEVELS-1). Must match Environment.cpp.
static const int PREF_LEVELS = 6;
static const float PREF_BLOCK_H = 64.0f; // texels per block (PrefilterHeight)

float3 SamplePrefiltered(float3 dir, float roughness)
{
    if (PrefIndex == NO_TEXTURE)
        return SampleEnvLod(dir, roughness * EnvMaxLod);

    float2 uv = DirToEquirect(dir);

    // Blend between the two adjacent roughness blocks (manual trilinear across the
    // atlas levels).
    float t = saturate(roughness) * (PREF_LEVELS - 1);
    float k0 = floor(t);
    float f = t - k0;

    const float invLevels = 1.0f / PREF_LEVELS;
    // Keep the vertical bilinear tap inside its block so it can't bleed across the
    // boundary into the neighbouring roughness level.
    float vb = clamp(uv.y, 0.5f / PREF_BLOCK_H, 1.0f - 0.5f / PREF_BLOCK_H);

    float2 uv0 = float2(uv.x, (k0 + vb) * invLevels);
    float2 uv1 = float2(uv.x, (k0 + 1.0f + vb) * invLevels);
    float3 c0 = textures[PrefIndex].SampleLevel(texSampler, uv0, 0).rgb;
    float3 c1 = textures[PrefIndex].SampleLevel(texSampler, uv1, 0).rgb;
    return lerp(c0, c1, f) * EnvIntensity;
}

// Specular IBL reflection: a crisp full-res env tap when nearly mirror (chrome,
// clear coat), fading into the smooth GGX-prefiltered atlas as roughness rises.
// The atlas convolution keeps matte reflections smooth where a box-mip would go
// blocky, while the sharp path preserves reflection detail on glossy surfaces.
float3 PrefilterEnv(float3 R, float roughness)
{
    float3 sharp = SampleEnvLod(R, roughness * EnvMaxLod);
    if (roughness < 0.12f)
        return sharp;
    float3 pref = SamplePrefiltered(R, roughness);
    return lerp(sharp, pref, smoothstep(0.12f, 0.35f, roughness));
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 Fr = max((1.0f - roughness).xxx, F0);
    return F0 + (Fr - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Karis' analytic environment-BRDF (split-sum) approximation -> no BRDF LUT.
float2 EnvBRDFApprox(float roughness, float NdotV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NdotV)) * r.x + r.y;
    return float2(-1.04f, 1.04f) * a004 + r.zw;
}

float4 main(PSInput input) : SV_Target0
{
    GPUMaterial mat = materials[pc.MaterialIndex];

    float4 baseColor = mat.BaseColorFactor;
    if (mat.TexIndex0.x != NO_TEXTURE)
        baseColor *= textures[mat.TexIndex0.x].Sample(texSampler, XformUV(input.TexCoord, mat.BaseColorTransform));

    // Alpha mask: discard fully-cut fragments (alphaMode == 1).
    uint alphaMode = mat.TexIndex1.y;
    if (alphaMode == 1u && baseColor.a < mat.RoughOccNormalCutoff.w)
        discard;

    float3 albedo = baseColor.rgb;
    float metallic = mat.EmissiveMetallic.w;
    float roughness = mat.RoughOccNormalCutoff.x;
    if (mat.TexIndex0.y != NO_TEXTURE)
    {
        float3 mr = SampleTex(mat.TexIndex0.y, XformUV(input.TexCoord, mat.MetallicRoughnessTransform));
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.045f, 1.0f);

    float ao = 1.0f;
    if (mat.TexIndex1.x != NO_TEXTURE)
        ao = lerp(1.0f, SampleTex(mat.TexIndex1.x, XformUV(input.TexCoord, mat.OcclusionTransform)).r, mat.RoughOccNormalCutoff.y);

    float3 emissive = mat.EmissiveMetallic.xyz;
    if (mat.TexIndex0.w != NO_TEXTURE)
        emissive *= SampleTex(mat.TexIndex0.w, XformUV(input.TexCoord, mat.EmissiveTransform));

    float3 Ngeom = normalize(input.Normal);
    if (!input.FrontFace)
        Ngeom = -Ngeom; // double-sided: face the geometric normal toward the viewer
    float3 N = Ngeom;
    float3 V = normalize(CameraPos - input.WorldPos);
    if (mat.TexIndex0.z != NO_TEXTURE)
    {
        float3 tn = SampleTex(mat.TexIndex0.z, XformUV(input.TexCoord, mat.NormalTransform)) * 2.0f - 1.0f;
        // Reduced amplitude keeps the mip-filtered flake as a subtle orange-peel
        // texture without the bright reflection amplifying it into coarse grain.
        tn.xy *= mat.RoughOccNormalCutoff.z * 0.3f;
        // TBN from the mesh's vertex tangent (Gram-Schmidt orthonormalise).
        float3 T = normalize(input.Tangent.xyz - Ngeom * dot(Ngeom, input.Tangent.xyz));
        float3 B = cross(Ngeom, T) * input.Tangent.w;
        float3x3 TBN = float3x3(T, B, Ngeom);
        N = normalize(mul(tn, TBN));
    }

    // Specular anti-aliasing: bump roughness by the screen-space normal variance
    // so the fine, high-frequency flake normal blurs in the reflection instead of
    // aliasing into sparkle/fireflies (Toksvig-style).
    float3 dNx = ddx(N);
    float3 dNy = ddy(N);
    float normalVar = sqrt(dot(dNx, dNx) + dot(dNy, dNy));
    roughness = saturate(roughness + min(normalVar * 0.7f, 0.4f));

    // Under a clear coat, the metallic flake base is a slightly matte layer. A
    // small roughness bump spreads the reflection and hides flake grain, but kept
    // low so the base still catches a bright, visible specular highlight (the sun)
    // rather than washing it out into a dim, over-broad glow.
    if (mat.Clearcoat.x > 0.0f)
        roughness = saturate(roughness + 0.05f);

    // KHR_materials_specular: tint/scale the dielectric reflectance. The base
    // dielectric F0 (0.04) is multiplied by the specular colour, and the whole
    // specular lobe is scaled by the specular weight. Metals keep F0 = albedo.
    float3 specColor = mat.Specular.rgb;
    if (mat.TexIndex1.w != NO_TEXTURE)
        specColor *= SampleTex(mat.TexIndex1.w, input.TexCoord);
    float specWeight = mat.Specular.w;
    if (mat.TexIndex1.z != NO_TEXTURE)
        specWeight *= textures[mat.TexIndex1.z].Sample(texSampler, input.TexCoord).a;

    float NdotV = saturate(dot(N, V)) + 1e-4f;
    float3 dielectricF0 = min(0.04f * specColor, 1.0f);
    float3 F0 = lerp(dielectricF0, albedo, metallic);
    // The reflection fades to the smooth geometric normal wherever the mapped
    // normal aliases within a pixel (the sub-pixel flake, high ddx/ddy) -- without
    // mipmaps that aliasing is what sparkles. Coarse, well-resolved normals (e.g.
    // carbon weave, low variance) keep their full reflection detail.
    float flakeFade = saturate(normalVar * 6.0f);
    float3 Nspec = normalize(lerp(N, Ngeom, max(saturate(roughness * 1.5f), flakeFade)));
    float3 R = reflect(-V, Nspec);

    // Pure image-based lighting from the HDR environment: diffuse from the baked
    // irradiance map, specular from the roughness-prefiltered reflection weighted
    // by the analytic environment BRDF. The HDR's own sun provides the highlight.
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kd = (1.0f - F) * (1.0f - metallic);

    float3 irradiance = SampleIrradiance(N);
    float3 diffuse = irradiance * albedo;

    float3 prefiltered = PrefilterEnv(R, roughness);
    float2 envBRDF = EnvBRDFApprox(roughness, NdotV);
    float3 specular = prefiltered * (F0 * envBRDF.x + envBRDF.y) * specWeight;

    // Transparent (blend) surfaces like tinted headlight glass barely diffuse --
    // they're dominated by specular reflection and transmission. Strongly fade their
    // diffuse so a tinted lens reads as a subtle glassy tint, not a flat saturated
    // block (e.g. the green headlight lens over the now-lit white DRL).
    float diffuseScale = mat.TexIndex1.y == 2u ? baseColor.a * 0.25f : 1.0f;

    float3 color = (kd * diffuse * diffuseScale + specular) * ao + emissive;

    // Analytic directional light (the sun): a Cook-Torrance specular lobe plus
    // Lambert diffuse on top of the image-based ambient. This resolves a crisp,
    // bright highlight that the prefiltered environment alone cannot.
    float3 L = normalize(LightDirection.xyz);
    float3 radiance = LightColor.rgb * LightColor.w;

    float NdotL = saturate(dot(N, L));
    if (NdotL > 0.0f)
    {
        float3 H = normalize(V + L);
        float NdotH = saturate(dot(N, H));
        float VdotH = saturate(dot(V, H));

        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        float3 Fs = FresnelSchlick(VdotH, F0);
        float3 specD = (D * G * Fs) / max(4.0f * NdotV * NdotL, 1e-4f) * specWeight;
        float3 kdD = (1.0f - Fs) * (1.0f - metallic);

        color += (kdD * albedo * diffuseScale / PI + specD) * radiance * NdotL * ao;
    }

    // Clearcoat (KHR_materials_clearcoat): a thin dielectric coat that reflects
    // the environment sharply over the base layer. It uses the geometric normal
    // (the coat is smooth -- it ignores the paint's flake), so the paint reads as
    // glossy wet coat on top of the metallic flake beneath.
    float cc = mat.Clearcoat.x;
    if (cc > 0.0f)
    {
        // The coat is a glossy wet lacquer: keep it near-mirror so it shows a crisp
        // reflection and bright highlights over the softer metallic-flake base. It
        // reflects off the smooth geometric normal, so the sharp tap stays clean
        // (no sub-pixel flake aliasing).
        float ccRough = clamp(mat.Clearcoat.y, 0.04f, 1.0f);
        float3 Rc = reflect(-V, Ngeom);
        float Fc = (0.04f + 0.96f * pow(saturate(1.0f - NdotV), 5.0f)) * cc;
        float3 ccSpec = PrefilterEnv(Rc, ccRough);
        color = color * (1.0f - Fc) + ccSpec * Fc;

        // Sharp direct sun glint through the coat.
        float NdotLc = saturate(dot(Ngeom, L));
        if (NdotLc > 0.0f)
        {
            float3 Hc = normalize(V + L);
            float NdotHc = saturate(dot(Ngeom, Hc));
            float VdotHc = saturate(dot(V, Hc));
            float ccD = DistributionGGX(NdotHc, ccRough);
            float ccG = GeometrySmith(NdotV, NdotLc, ccRough);
            float ccFd = (0.04f + 0.96f * pow(saturate(1.0f - VdotHc), 5.0f)) * cc;
            float ccSpecD = ccD * ccG * ccFd / max(4.0f * NdotV * NdotLc, 1e-4f);
            color += ccSpecD * radiance * NdotLc;
        }
    }

    color = ACESFilm(color);

    // Screen-space refraction (glass transmission): composite the grabbed scene
    // colour from behind the surface, nudged by the view-space normal to fake
    // refraction and tinted by the glass colour. A Fresnel term keeps the
    // reflection at grazing angles and transmits head-on. Done after tonemap since
    // the grabbed scene is already display-space.
    float transmission = mat.Clearcoat.z;
    if (transmission > 0.0f && SceneColorIndex != NO_TEXTURE)
    {
        float2 uv = input.ClipPos.xy / float2(ScreenWidth, ScreenHeight);
        float3 viewN = mul((float3x3)View, N);
        uv += viewN.xy * float2(0.02f, -0.02f);

        // Frosted glass: rough surfaces scatter the transmitted light, so blur the
        // grabbed scene with a Vogel disk whose radius grows with roughness. Smooth
        // glass (roughness ~0) takes a single sharp tap.
        float3 behind;
        if (roughness < 0.1f)
        {
            behind = textures[SceneColorIndex].SampleLevel(texSampler, saturate(uv), 0).rgb;
        }
        else
        {
            float radius = roughness * 0.06f;
            float3 sum = float3(0.0f, 0.0f, 0.0f);
            const int N = 8;
            [unroll] for (int i = 0; i < N; ++i)
            {
                float r = sqrt((i + 0.5f) / N) * radius;
                float a = i * 2.39996323f;
                float2 off = float2(cos(a), sin(a)) * r;
                sum += textures[SceneColorIndex].SampleLevel(texSampler, saturate(uv + off), 0).rgb;
            }
            behind = sum / N;
        }

        float3 tint = lerp(float3(1.0f, 1.0f, 1.0f), baseColor.rgb, 0.05f);
        behind *= tint;

        float fresnel = 0.04f + 0.96f * pow(saturate(1.0f - NdotV), 5.0f);
        color = lerp(color, behind, transmission * (1.0f - fresnel));
    }

    return float4(color, baseColor.a);
}
