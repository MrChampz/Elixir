struct PSInput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;
};

float4 main(PSInput input) : SV_Target0
{
    float2 pointCoord = float2(0.5f, 0.5f);
    float2 centered = (pointCoord * 2.0f) - 1.0f;
    float alpha = smoothstep(1.0, 0.35, dot(centered, centered));
    return float4(input.Color.rgb, input.Color.a * alpha);
}