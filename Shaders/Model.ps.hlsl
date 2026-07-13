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
    uint4 TexIndex1;             // occlusion, unused...
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
    float2 TexCoord  : TEXCOORD0;
    float3 WorldPos  : POSITION0;
    bool FrontFace   : SV_IsFrontFace;
};

static const float PI = 3.14159265359f;
static const uint NO_TEXTURE = 0xffffffffu;

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

// Cotangent frame from screen-space derivatives -> tangent-space normal mapping
// without a mesh TANGENT attribute.
float3 PerturbNormal(float3 N, float3 worldPos, float2 uv, float3 texNormal)
{
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    float3x3 TBN = float3x3(T * invmax, B * invmax, N);
    return normalize(mul(texNormal, TBN));
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(PSInput input) : SV_Target0
{
    GPUMaterial mat = materials[pc.MaterialIndex];

    float4 baseColor = mat.BaseColorFactor;
    if (mat.TexIndex0.x != NO_TEXTURE)
        baseColor *= textures[mat.TexIndex0.x].Sample(texSampler, input.TexCoord);

    // Alpha mask: discard fully-cut fragments (alphaMode == 1).
    uint alphaMode = mat.TexIndex1.y;
    if (alphaMode == 1u && baseColor.a < mat.RoughOccNormalCutoff.w)
        discard;

    float3 albedo = baseColor.rgb;
    float metallic = mat.EmissiveMetallic.w;
    float roughness = mat.RoughOccNormalCutoff.x;
    if (mat.TexIndex0.y != NO_TEXTURE)
    {
        float3 mr = SampleTex(mat.TexIndex0.y, input.TexCoord);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.045f, 1.0f);

    float ao = 1.0f;
    if (mat.TexIndex1.x != NO_TEXTURE)
        ao = lerp(1.0f, SampleTex(mat.TexIndex1.x, input.TexCoord).r, mat.RoughOccNormalCutoff.y);

    float3 emissive = mat.EmissiveMetallic.xyz;
    if (mat.TexIndex0.w != NO_TEXTURE)
        emissive *= SampleTex(mat.TexIndex0.w, input.TexCoord);

    float3 N = normalize(input.Normal);
    if (!input.FrontFace)
        N = -N; // double-sided: face the geometric normal toward the viewer
    float3 V = normalize(CameraPos - input.WorldPos);
    if (mat.TexIndex0.z != NO_TEXTURE)
    {
        float3 tn = SampleTex(mat.TexIndex0.z, input.TexCoord) * 2.0f - 1.0f;
        tn.xy *= mat.RoughOccNormalCutoff.z;
        N = PerturbNormal(N, input.WorldPos, input.TexCoord, tn);
    }

    float3 F0 = lerp(0.04f.xxx, albedo, metallic);

    // Single directional key light.
    float3 L = normalize(float3(0.4f, 0.85f, 0.35f));
    float3 radiance = float3(1.0f, 0.98f, 0.94f) * 3.0f;

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

    // Flat ambient (stand-in for IBL): diffuse fills unlit areas, a small
    // Fresnel-tinted term keeps metals from going black without reflections.
    float3 ambientColor = float3(0.13f, 0.15f, 0.19f);
    float3 ambient = (albedo * (1.0f - metallic) + F0) * ambientColor * ao;

    float3 color = ambient + Lo + emissive;
    color = ACESFilm(color);

    return float4(color, baseColor.a);
}
