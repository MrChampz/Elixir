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

static const float3 SUN_DIRECTION = normalize(float3(0.4f, 0.85f, 0.35f));
static const float3 SUN_RADIANCE = float3(1.0f, 0.97f, 0.92f);

// Procedural environment used as the IBL source: a smooth gradient sky (warm
// horizon -> blue zenith, darker ground) with only a soft, broad glow toward the
// sun. Deliberately has NO sharp sun disk: the crisp sun highlight comes from the
// analytic directional light instead, so a normal-mapped glossy surface reflects
// a smooth environment rather than scattering a sharp sun into speckled cells.
float3 SampleEnvironment(float3 dir)
{
    float up = dir.y;
    float3 zenith = float3(0.14f, 0.26f, 0.52f);
    float3 horizon = float3(0.55f, 0.58f, 0.64f);
    float3 ground = float3(0.08f, 0.075f, 0.07f);
    float3 sky = lerp(horizon, zenith, pow(saturate(up), 0.55f));
    float3 env = up >= 0.0f ? sky : lerp(horizon, ground, saturate(-up * 3.0f));

    float d = saturate(dot(dir, SUN_DIRECTION));
    env += SUN_RADIANCE * pow(d, 6.0f) * 0.15f; // soft, broad warmth only
    return env;
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

    float3 N = normalize(input.Normal);
    if (!input.FrontFace)
        N = -N; // double-sided: face the geometric normal toward the viewer
    float3 V = normalize(CameraPos - input.WorldPos);
    if (mat.TexIndex0.z != NO_TEXTURE)
    {
        float3 tn = SampleTex(mat.TexIndex0.z, XformUV(input.TexCoord, mat.NormalTransform)) * 2.0f - 1.0f;
        tn.xy *= mat.RoughOccNormalCutoff.z;
        // TBN from the mesh's vertex tangent (Gram-Schmidt orthonormalise).
        float3 T = normalize(input.Tangent.xyz - N * dot(N, input.Tangent.xyz));
        float3 B = cross(N, T) * input.Tangent.w;
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(tn, TBN));
    }

    float3 F0 = lerp(0.04f.xxx, albedo, metallic);

    // Single directional key light (matches the sun in the environment).
    float3 L = SUN_DIRECTION;
    float3 radiance = SUN_RADIANCE * 3.0f;

    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V)) + 1e-4f;
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(VdotH, F0);

    float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
    float3 kd = (1.0f - F) * (1.0f - metallic);
    float3 Lo = (kd * albedo / PI + specular) * radiance * NdotL;

    // Image-based lighting from the procedural environment: diffuse irradiance
    // in the normal direction, and a roughness-blurred reflection weighted by the
    // analytic environment BRDF -- this is what makes the metals reflect the sky.
    float3 R = reflect(-V, N);
    float3 irradiance = SampleEnvironment(N);
    float3 prefiltered = lerp(SampleEnvironment(R), irradiance, roughness);
    float2 envBRDF = EnvBRDFApprox(roughness, NdotV);
    float3 iblDiffuse = irradiance * albedo * (1.0f - metallic);
    float3 iblSpecular = prefiltered * (F0 * envBRDF.x + envBRDF.y);
    float3 ambient = (iblDiffuse + iblSpecular) * ao;

    float3 color = ambient + Lo + emissive;
    color = ACESFilm(color);

    return float4(color, baseColor.a);
}
