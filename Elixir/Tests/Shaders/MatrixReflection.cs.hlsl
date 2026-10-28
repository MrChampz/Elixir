struct FloatValues
{
    float F1;
    float2 F2;
    float3 F3;
    float4 F4;
    float3x3 M3;
    float4x4 M4;
};

[[vk::binding(0, 0)]]
cbuffer constants
{
    float F1;
    float2 F2;
    float3 F3;
    float4 F4;
    float3x3 M3;
    float4x4 M4;
    FloatValues Nested;
};

[[vk::push_constant]]
FloatValues pushValues;

[[vk::binding(1, 0)]]
StructuredBuffer<FloatValues> nestedInput;

[[vk::binding(2, 0)]]
StructuredBuffer<float3x3> matrixInput;

[[vk::binding(3, 0)]]
RWStructuredBuffer<float> outputValues;

float Sum(FloatValues value)
{
    return value.F1 + value.F2.y + value.F3.z + value.F4.w
        + value.M3[2][2] + value.M4[3][3];
}

[numthreads(1, 1, 1)]
void main()
{
    outputValues[0] = F1 + F2.y + F3.z + F4.w + M3[2][2] + M4[3][3]
        + Sum(Nested) + Sum(pushValues) + Sum(nestedInput[0]) + matrixInput[0][2][2];
}
