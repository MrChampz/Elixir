[[vk::binding(0, 0)]]
cbuffer constants
{
    float2x2 Matrix;
};

[[vk::binding(1, 0)]]
RWStructuredBuffer<float> outputValues;

[numthreads(1, 1, 1)]
void main()
{
    outputValues[0] = Matrix[1][1];
}
