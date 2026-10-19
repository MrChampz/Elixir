[[vk::binding(0, 0)]]
cbuffer constants
{
    uint4 Value;
};

[[vk::binding(1, 0)]]
StructuredBuffer<uint> inputValues;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> outputValues;

[numthreads(1, 1, 1)]
void main()
{
    outputValues[0] = Value.x + inputValues[1];
}
