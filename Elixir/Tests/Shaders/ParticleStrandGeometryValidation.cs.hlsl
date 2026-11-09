#define main EvaluateStrandVertex
#include "../../../Shaders/Material/ParticleStrand.vs.hlsl"
#undef main

[[vk::binding(2, 0)]] RWStructuredBuffer<float4> results;

[numthreads(8, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.ParticleCount)
        return;
    VSOutput vertex = EvaluateStrandVertex(0, id.x);
    results[id.x * 2] = float4(vertex.WorldPos, vertex.Valid);
    results[id.x * 2 + 1] = float4(vertex.Normal, vertex.Color.a);
}
