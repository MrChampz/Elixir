#define main EvaluateSpriteVertex
#include "../../../Shaders/Material/ParticleSprite.vs.hlsl"
#undef main

[[vk::binding(2, 0)]] RWStructuredBuffer<float4> results;

[numthreads(6, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const VSInput particle = (VSInput)0;
    const VSOutput vertex = EvaluateSpriteVertex(particle, id.x);
    results[id.x] = float4(vertex.TexCoord, vertex.TexCoord1);
}
