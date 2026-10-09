#include "ParticleData.hlsl"
#include "ParticleAttributes.hlsl"
#include "ParticleSpawnCommon.hlsl"

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;
    float4 ViewportData;
};

struct PushConstants {
    uint InstanceIndex;
    uint EmitterIndex;
};

[[vk::push_constant]]
PushConstants pc;

[[vk::binding(0, 0)]]
RWStructuredBuffer<SGPUParticle> particles;

[[vk::binding(1, 0)]]
StructuredBuffer<SGPUEmitter> emitters;

[[vk::binding(3, 0)]]
ByteAddressBuffer parameters;

[[vk::binding(4, 0)]]
StructuredBuffer<SGPUSystemInstance> instances;

[[vk::binding(6, 0)]]
StructuredBuffer<SGPUSpawnRequest> spawnRequests;

float Hash1(float x) {
  return frac(sin(x * 91.3458 + 12.345) * 45678.5453);
}





float ResolveDynamicInput(uint inputType, float randomValue, float particleSeed)
{
    if (inputType == 1u) // DeltaTime
        return TimeData.x;

    if (inputType == 2u) // NormalizedAge
        return 0.0;

    if (inputType == 3u) // EmitterTime
        return TimeData.y;

    if (inputType == 4u) // Random
        return randomValue;

    if (inputType == 5u) // ParticleSeed
        return particleSeed;

    return 1.0;
}



#include "Operation.hlsl"

[[vk::binding(2, 0)]]
StructuredBuffer<SGPUOperation> operations;

[[vk::binding(7, 0)]]
StructuredBuffer<uint> operands;

[[vk::binding(8, 0)]]
ByteAddressBuffer constants;

#include "OperationExecutor.hlsl"
#include "Operations/RandomSampling.hlsl"

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SGPUSystemInstance instance = instances[pc.InstanceIndex];

    uint emitterIndex = instance.EmitterBaseOffset + pc.EmitterIndex;
    SGPUEmitter emitter = emitters[emitterIndex];

    uint spawnOrder = dispatchThreadId.x;
    uint emitterCount = emitter.MaxParticles;
    if (spawnOrder >= emitterCount)
        return;

    SGPUSpawnRequest request = spawnRequests[instance.SpawnRequestBaseOffset + pc.EmitterIndex];
    if (request.Generation != instance.Generation)
        return;

    uint spawnCursor = request.SpawnCursor;
    uint spawnCount = request.SpawnCount;

    if (spawnOrder >= spawnCount)
        return;

    // Keep new particles in consecutive lanes even when the ring cursor wraps.
    uint remaining = emitterCount - spawnCursor;
    uint localIndex = spawnOrder < remaining
        ? spawnCursor + spawnOrder
        : spawnOrder - remaining;

    uint globalIndex = instance.ParticleBaseOffset + emitter.LocalParticleOffset + localIndex;
    float2 seedBase = float2((float)globalIndex, TimeData.y + float(spawnCursor));
    float particleSeed = Hash1((float)globalIndex);
    float randomInput = Hash2(seedBase + float2(8.11, 3.41));
    uint emissionIndex = request.EmissionIndex + spawnOrder;

    AttributeTable attributes = EmptyAttributeTable();

    uint operationOffset = emitter.SpawnOperationOffset;
    uint operationCount = emitter.SpawnOperationCount;

    SOperationExecutor executor = CreateOperationExecutor(operands, constants, parameters);

    SOperationExecutorContext context = (SOperationExecutorContext)0;
    context.Instance = instance;
    context.ParticleSeed = particleSeed;
    context.Attributes = attributes;
    context.Time = TimeData.y;
    context.SeedBase = seedBase;
    context.SpawnRate = emitter.SpawnRatePerSecond;
    context.SpawnCount = spawnCount;
    context.SpawnOrder = spawnOrder;
    context.EmissionIndex = emissionIndex;

    for (uint i = 0u; i < operationCount; ++i)
    {
        context.OperationIndex = i;
        SGPUOperation operation = operations[operationOffset + i];

        executor.Execute(operation, context);
    }

    attributes = context.Attributes;

    particles[globalIndex].Position = GetAttribute(attributes, ParticleAttribute_Position).xyz;
    particles[globalIndex].Size = GetAttribute(attributes, ParticleAttribute_Size).x;
    particles[globalIndex].Velocity = GetAttribute(attributes, ParticleAttribute_Velocity).xyz;
    particles[globalIndex].Age = 0.0;
    particles[globalIndex].Transform = float4(GetAttribute(attributes, ParticleAttribute_Rotation).x, GetAttribute(attributes, ParticleAttribute_Scale).x, 0.0, 0.0);
    particles[globalIndex].Tangent = GetAttribute(attributes, ParticleAttribute_Tangent).xyz;
    particles[globalIndex].RibbonId = attributes.RibbonId;
    particles[globalIndex].Color = GetAttribute(attributes, ParticleAttribute_Color);
    particles[globalIndex].EmitterIndex = pc.EmitterIndex;
    particles[globalIndex].LinkOrder = emissionIndex;
    particles[globalIndex].Lifetime = GetAttribute(attributes, ParticleAttribute_Lifetime).x;
    particles[globalIndex].Alive = 1.0;
}
