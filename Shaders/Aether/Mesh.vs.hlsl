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
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct VSInput
{
    float3 LocalPos      : POSITION0;
    float3 LocalNormal   : NORMAL0;
    float4 PositionSize  : POSITION1;
    float4 VelocityAge   : TEXCOORD0;
    float4 Tangent       : TANGENT;
    float4 Color         : COLOR;
    float4 Metadata      : TEXCOORD1;
};

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float4 Color : COLOR0;
    float3 Normal : NORMAL0;
    float3 WorldPos : POSITION0;
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

    uint id = asuint(input.Metadata.y);
    float alive = input.Metadata.w >= 0.5 ? 1.0 : 0.0;
    float age = input.VelocityAge.w;
    float lifetime = max(input.Metadata.z, 0.001);
    float lifeAlpha = clamp(age / lifetime, 0.0, 1.0);
    float seed  = Hash01(id);
    float phase = Hash01(id ^ 0x9e3779b9u);

    float scale = lerp(0.045, 0.085, clamp(input.PositionSize.w / 18.0, 0.0, 1.0));
    scale *= lerp(1.0, 0.55, lifeAlpha);

    float rotX = (seed * 5.1) + age * lerp(-0.9, 1.1, frac(seed * 9.7));
    float rotY = (seed * 8.4) + age * lerp(0.6, 1.4, frac(seed * 3.1));
    float rotZ = (phase * 6.28318) + age * 0.45;
    float3x3 rotation = mul(RotationY(rotY), mul(RotationX(rotX), RotationZ(rotZ)));

    float3 worldPos = input.PositionSize.xyz + mul(mul(rotation, input.LocalPos * scale), alive);
    float3 worldNormal = normalize(mul(rotation, input.LocalNormal));

    output.ClipPos = mul(ViewProj, float4(worldPos, 1.0));
    output.Color = float4(input.Color.rgb, input.Color.a * alive);
    output.Normal = worldNormal;
    output.WorldPos = worldPos;

    return output;
}