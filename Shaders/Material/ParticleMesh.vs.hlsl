// Template for EMaterialUsage::ParticleMesh. It mirrors Aether/Mesh.vs.hlsl
// while exposing MaterialIndex and planar UVs to the generated fragment graph.

float3x3 RotationX(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return float3x3(
        1.0, 0.0, 0.0,
        0.0, c,   -s,
        0.0, s,   c
    );
}

float3x3 RotationY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return float3x3(
        c,   0.0, s,
        0.0, 1.0, 0.0,
        -s,  0.0, c
    );
}

float3x3 RotationZ(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return float3x3(
        c,   -s,  0.0,
        s,   c,   0.0,
        0.0, 0.0, 1.0
    );
}

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4    View;
    float4x4    Proj;
    float4x4    ViewProj;
    float3      CameraPos;
    float       Time;
};

struct MaterialPushConstants
{
    float4x4    WorldTransform;
    uint        MaterialIndex;
};

[[vk::push_constant]]
MaterialPushConstants pc;

struct VSInput
{
    float3 LocalPos        : POSITION0;
    float3 LocalNormal     : NORMAL0;
    float4 PositionSize    : POSITION1;
    float4 VelocityAge     : TEXCOORD0;
    float4 Transform       : TEXCOORD1;
    float4 TangentRibbonId : TANGENT;
    float4 Color           : COLOR;
    float4 Metadata        : TEXCOORD2;
};

struct VSOutput
{
    float4 ClipPos  : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 Normal   : NORMAL0;
    float4 Color    : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

float Hash01(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return float(x) * (1.0 / 4294967296.0); // / 2^32
}

VSOutput main(VSInput input)
{
    VSOutput output;

    const uint id = asuint(input.Metadata.y);
    const float alive = input.Metadata.w >= 0.5 ? 1.0 : 0.0;
    const float seed  = Hash01(id);
    const float phase = Hash01(id ^ 0x9e3779b9u);

    float scale = lerp(0.045, 0.085, clamp(input.PositionSize.w / 18.0, 0.0, 1.0));
    scale *= max(input.Transform.y, 0.0);

    const float baseRotation = input.Transform.x;
    const float rotX = baseRotation + (seed * 1.7);
    const float rotY = baseRotation * 0.7 + (phase * 6.28318);
    const float rotZ = baseRotation * 1.2 + (seed * 2.3);
    const float3x3 rotation = mul(RotationY(rotY), mul(RotationX(rotX), RotationZ(rotZ)));

    const float3 localPos = input.PositionSize.xyz +
        mul(mul(rotation, input.LocalPos * scale), alive);
    const float3 localNormal = normalize(mul(rotation, input.LocalNormal));

    float3 worldPos = mul(pc.WorldTransform, float4(localPos, 1.0f)).xyz;
    float3 worldNormal = normalize(mul((float3x3)pc.WorldTransform, localNormal));

    output.WorldPos = mul(pc.WorldTransform, float4(localPos, 1.0f)).xyz;
    output.ClipPos = mul(ViewProj, float4(output.WorldPos, 1.0));
    output.Normal = normalize(mul((float3x3)pc.WorldTransform, localNormal));
    output.Color = float4(input.Color.rgb, input.Color.a * alive);
    output.TexCoord = input.LocalPos.xy * 0.5f + 0.5f;

    return output;
}