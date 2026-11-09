[[vk::binding(0, 0)]] cbuffer cbFrame
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
};
struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
    float4 Color : COLOR;
    nointerpolation float Valid : TEXCOORD0;
};
float4 main(PSInput input) : SV_Target0
{
    clip(input.Valid - .5);
    clip(input.Color.a - .001);
    float3 normal = normalize(input.Normal);
    float3 view = normalize(CameraPos - input.WorldPos);
    float3 light = normalize(float3(-.45,.8,.55));
    float diffuse = max(dot(normal,light),0);
    float3 halfDirection = normalize(light + view);
    float specular = pow(max(dot(normal,halfDirection),0),65);
    float broadHighlight = pow(max(dot(normal,normalize(float3(.1,.9,.55) + view)),0),15);
    float fresnel = pow(1.0-saturate(dot(normal,view)),4);
    float3 color = input.Color.rgb * (.22 + .78 * diffuse);
    color += float3(1,1,.98) * (specular * 1.1 + broadHighlight * .25);
    color += float3(.2,.3,.45) * fresnel * .22;
    return float4(color,1);
}
