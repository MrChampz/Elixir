[[vk::binding(0, 0)]] cbuffer cbFrame
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
};
struct PushConstants
{
    uint ParticleOffset;
    uint PointOffset;
    uint PointCount;
    uint BranchCount;
    uint LeafInterval;
    float LeafLength;
    float LeafWidth;
    float MaturationSegments;
    float4 LeafColor;
    uint LeafShape;
};
[[vk::push_constant]] PushConstants pc;
struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
    float4 Color : COLOR;
    nointerpolation float Valid : TEXCOORD2;
};
// Distance to a vein segment in leaf UV coordinates.
float veinDistance(float2 uv, float2 start, float2 end) {
    float2 edge = end - start;
    float t = saturate(dot(uv - start, edge) / max(dot(edge, edge), .000001));
    return length(uv - start - edge * t);
}

float4 main(PSInput input) : SV_Target0
{
    clip(input.Valid - .5);
    clip(input.Color.a - .001);
    float3 baseColor = input.Color.rgb;
    {
        float2 uv = input.TexCoord;
        float2 base = float2(.5,.2);
        float vein = veinDistance(uv, base, float2(.5,1));
        if (pc.LeafShape == 0u) {
        vein = min(vein, veinDistance(uv, base, float2(.02,.28)));
        vein = min(vein, veinDistance(uv, base, float2(.98,.28)));
        vein = min(vein, veinDistance(uv, base, float2(-.02,.65)));
        vein = min(vein, veinDistance(uv, base, float2(1.02,.65)));
        }
        float width = max(fwidth(vein), .0015);
        float veins = 1.0 - smoothstep(.002, .006 + width, vein);
        float mottling = .96 + .04 * sin(uv.x * 71 + uv.y * 43) * sin(uv.y * 59);
        baseColor *= mottling * lerp(.82, 1.18, saturate(uv.y));
        baseColor = lerp(baseColor, baseColor * 1.6 + float3(.025,.045,.008), veins * .55);
    }

    float3 N = normalize(input.Normal);
    float3 V = normalize(CameraPos - input.WorldPos);
    float diffuse = max(dot(N, normalize(float3(-.45,.8,.55))), 0.0);
    float rim = pow(1.0 - max(dot(N,V),0.0),2.8);
    return float4(baseColor * (.24 + diffuse * .92) + float3(.35,.42,.52) * rim * .35, 1);
}
