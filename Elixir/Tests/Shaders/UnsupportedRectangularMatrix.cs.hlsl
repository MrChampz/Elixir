[[vk::binding(0, 0)]]
cbuffer constants
{
    float3x4 Matrix;
};

[[vk::binding(1, 0)]]
RWStructuredBuffer<float> outputValues;

[numthreads(1, 1, 1)]
void main()
{
    outputValues[0] = Matrix[2][3];
}
