struct IntegerVectors
{
    int I1;
    int2 I2;
    int3 I3;
    int4 I4;
    uint U1;
    uint2 U2;
    uint3 U3;
    uint4 U4;
};

[[vk::binding(0, 0)]]
cbuffer constants
{
    int I1;
    int2 I2;
    int3 I3;
    int4 I4;
    uint U1;
    uint2 U2;
    uint3 U3;
    uint4 U4;
    IntegerVectors Nested;
};

[[vk::push_constant]]
IntegerVectors pushValues;

[[vk::binding(1, 0)]]
StructuredBuffer<IntegerVectors> nestedInput;

[[vk::binding(2, 0)]]
StructuredBuffer<int2> signedInput;

[[vk::binding(3, 0)]]
StructuredBuffer<uint4> unsignedInput;

[[vk::binding(4, 0)]]
RWStructuredBuffer<uint> outputValues;

uint Sum(IntegerVectors value)
{
    return asuint(value.I1 + value.I2.y + value.I3.z + value.I4.w)
        + value.U1 + value.U2.y + value.U3.z + value.U4.w;
}

[numthreads(1, 1, 1)]
void main()
{
    outputValues[0] = Sum(Nested) + Sum(pushValues) + Sum(nestedInput[0])
        + asuint(I1 + I2.y + I3.z + I4.w) + U1 + U2.y + U3.z + U4.w
        + asuint(signedInput[0].y) + unsignedInput[0].w;
}
