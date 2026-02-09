struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float4 Color   : COLOR;
};

float4 main(PSInput input) : SV_Target0
{
    return input.Color;
}