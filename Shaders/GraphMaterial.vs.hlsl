// Vertex shader template for node-graph materials. The graph's WorldPositionOffset
// channel is injected at __WPO_BODY__ to displace the vertex in world space before
// projection. Mirrors Model.vs output so the graph pixel shader consumes it.

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
};

[[vk::binding(1, 0)]]
SamplerState texSampler : register(s0);

struct GPUMaterial
{
    float4 BaseColorFactor;
    float4 EmissiveMetallic;
    float4 RoughOccNormalCutoff;
    uint4 TexIndex0;
    uint4 TexIndex1;
    float4 BaseColorTransform;
    float4 MetallicRoughnessTransform;
    float4 NormalTransform;
    float4 EmissiveTransform;
    float4 OcclusionTransform;
    float4 Clearcoat;
    float4 Specular;
};

[[vk::binding(2, 0)]]
StructuredBuffer<GPUMaterial> materials;

[[vk::binding(3, 0)]]
cbuffer cbGraphParams : register(b1)
{
    float4 GraphParams[8];
};

[[vk::binding(1, 1)]]
Texture2D textures[] : register(t0);

struct ModelPushConstants
{
    float4x4 Model;
    uint MaterialIndex;
};
[[vk::push_constant]]
ModelPushConstants pc;

static const uint NO_TEXTURE = 0xffffffffu;

// Vertex stage has no implicit derivatives -> sample an explicit LOD.
float3 SampleTex(uint index, float2 uv)
{
    return textures[index].SampleLevel(texSampler, uv, 0).rgb;
}

float Checker(float2 uv, float scale)
{
    float2 c = floor(uv * scale);
    return frac((c.x + c.y) * 0.5f) < 0.25f ? 0.0f : 1.0f;
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 uv, float scale)
{
    uv *= scale;
    float2 i = floor(uv);
    float2 f = frac(uv);
    f = f * f * (3.0f - 2.0f * f);
    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float Checker3(float3 p, float scale)
{
    float3 c = floor(p * scale);
    return frac((c.x + c.y + c.z) * 0.5f) < 0.25f ? 0.0f : 1.0f;
}

float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.zyx + 31.32f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3(float3 pos, float scale)
{
    pos *= scale;
    float3 i = floor(pos);
    float3 f = frac(pos);
    f = f * f * (3.0f - 2.0f * f);
    float x00 = lerp(Hash31(i + float3(0, 0, 0)), Hash31(i + float3(1, 0, 0)), f.x);
    float x10 = lerp(Hash31(i + float3(0, 1, 0)), Hash31(i + float3(1, 1, 0)), f.x);
    float x01 = lerp(Hash31(i + float3(0, 0, 1)), Hash31(i + float3(1, 0, 1)), f.x);
    float x11 = lerp(Hash31(i + float3(0, 1, 1)), Hash31(i + float3(1, 1, 1)), f.x);
    return lerp(lerp(x00, x10, f.y), lerp(x01, x11, f.y), f.z);
}

struct VSInput
{
    float3 Position : POSITION0;
    float3 Normal   : NORMAL0;
    float4 Tangent  : TANGENT0;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 ClipPos  : SV_POSITION;
    float3 Normal   : NORMAL0;
    float4 Tangent  : TANGENT0;
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : POSITION0;
};

// The graph's World Position Offset as a function of world position and UV, so it
// can be sampled at neighbouring points to recompute the surface normal.
float3 ComputeWPO(float3 P, float2 uv)
{
    GPUMaterial mat = materials[pc.MaterialIndex];
    float3 wpo = float3(0.0f, 0.0f, 0.0f);

    // __WPO_BODY__

    // Keep the materials / cbGraphParams bindings alive against DXC dead-code
    // elimination even when the graph reads neither (imperceptible).
    wpo += mat.BaseColorFactor.rgb * 1e-8f + GraphParams[0].rgb * 1e-8f;
    return wpo;
}

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(pc.Model, float4(input.Position, 1.0f));
    float3 N0 = normalize(mul((float3x3)pc.Model, input.Normal));

    float3 p0 = worldPos.xyz + ComputeWPO(worldPos.xyz, input.TexCoord);

    // Recompute the normal from the displaced surface via finite differences along
    // the tangent basis, so the lighting follows the deformation. Falls back to the
    // geometric normal when there's no tangent or no displacement.
    float3 N = N0;
    if (dot(input.Tangent.xyz, input.Tangent.xyz) > 1e-8f)
    {
        float3 T = normalize(mul((float3x3)pc.Model, input.Tangent.xyz));
        T = normalize(T - N0 * dot(N0, T)); // Gram-Schmidt against N0
        float3 B = cross(N0, T) * input.Tangent.w;

        const float eps = 0.01f;
        float3 pT = (worldPos.xyz + T * eps) + ComputeWPO(worldPos.xyz + T * eps, input.TexCoord);
        float3 pB = (worldPos.xyz + B * eps) + ComputeWPO(worldPos.xyz + B * eps, input.TexCoord);

        float3 fn = cross(pT - p0, pB - p0);
        if (dot(fn, fn) > 1e-12f)
        {
            fn = normalize(fn);
            N = dot(fn, N0) < 0.0f ? -fn : fn; // keep the original orientation
        }
    }

    output.ClipPos = mul(ViewProj, float4(p0, 1.0f));
    output.WorldPos = p0;
    output.Normal = N;
    output.Tangent = float4(mul((float3x3)pc.Model, input.Tangent.xyz), input.Tangent.w);
    output.TexCoord = input.TexCoord;

    return output;
}
