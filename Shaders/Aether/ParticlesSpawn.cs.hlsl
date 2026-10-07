struct ParticleState
{
    float4 PositionSize;    // xyz = position, w = size
    float4 VelocityAge;     // xyz = velocity, w = age
    float4 Transform;       // x = rotation, y = scale
    float3 Tangent;
    uint   RibbonId;
    float4 Color;
    float4 Metadata;        // x = emitter index, y = ribbon link order, z = lifetime, w = alive
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 MetaA; // x = offset in particle buffer, y = max particles, z = operation offset(spawn), w = operation count(spawn)
    float4 MetaB; // x = operation offset(update), y = operation count(update), z = buffer cursor, w = spawn count
    float4 MetaC; // x = render mode, y = spawn rate seconds, z = gravity scale, w = next buffer cursor
	float4 MetaD; // x = emission index
};

[[vk::binding(1, 0)]]
StructuredBuffer<Emitter> emitters;

struct Parameter
{
    float4 Value;
};

[[vk::binding(3, 0)]]
StructuredBuffer<Parameter> parameters;

struct SystemInstance
{
    uint ParticleBaseOffset;
    uint EmitterBaseOffset;
    uint OperationBaseOffset;
    uint OperandBaseOffset;
    uint ConstantBaseOffset;
    uint ParameterBaseOffset;

    uint EmitterStateBaseOffset;
    uint SpawnRequestBaseOffset;
    uint TriggerEventBaseOffset;
    uint TriggerQueueStateBaseOffset;

    uint ParticleCount;
    uint EmitterCount;
    uint TriggerEventCapacityPerEmitter;
    uint Generation;

    uint ParticleStateLayoutIndex;
};

[[vk::binding(4, 0)]]
StructuredBuffer<SystemInstance> instances;

struct SpawnRequest
{
    uint SpawnCursor;
    uint SpawnCount;
    uint EmissionIndex;
    uint Generation;
};

[[vk::binding(6, 0)]]
StructuredBuffer<SpawnRequest> spawnRequests;

#include "ParticleAttributes.hlsl"

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
    SystemInstance instance = instances[pc.InstanceIndex];

    uint emitterIndex = instance.EmitterBaseOffset + pc.EmitterIndex;
    Emitter emitter = emitters[emitterIndex];

    uint spawnOrder = dispatchThreadId.x;
    uint emitterCount = (uint)emitter.MetaA.y;
    if (spawnOrder >= emitterCount)
        return;

    SpawnRequest request = spawnRequests[instance.SpawnRequestBaseOffset + pc.EmitterIndex];
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

    uint globalIndex = instance.ParticleBaseOffset + (uint)emitter.MetaA.x + localIndex;
    float2 seedBase = float2((float)globalIndex, TimeData.y + float(spawnCursor));
    float particleSeed = Hash1((float)globalIndex);
    float randomInput = Hash2(seedBase + float2(8.11, 3.41));
    uint emissionIndex = request.EmissionIndex + spawnOrder;

    AttributeTable attributes = EmptyAttributeTable();

    uint operationOffset = (uint)emitter.MetaA.z;
    uint operationCount = (uint)emitter.MetaA.w;

    SOperationExecutor executor = CreateOperationExecutor(operands, constants, parameters);

    SOperationExecutorContext context = (SOperationExecutorContext)0;
    context.Instance = instance;
    context.ParticleSeed = particleSeed;
    context.Attributes = attributes;
    context.Time = TimeData.y;
    context.SeedBase = seedBase;
    context.SpawnRate = emitter.MetaC.y;
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

    particles[globalIndex].PositionSize = float4(GetAttribute(attributes, ParticleAttribute_Position).xyz, GetAttribute(attributes, ParticleAttribute_Size).x);
    particles[globalIndex].VelocityAge = float4(GetAttribute(attributes, ParticleAttribute_Velocity).xyz, 0.0);
    particles[globalIndex].Transform = float4(GetAttribute(attributes, ParticleAttribute_Rotation).x, GetAttribute(attributes, ParticleAttribute_Scale).x, 0.0, 0.0);
    particles[globalIndex].Tangent = GetAttribute(attributes, ParticleAttribute_Tangent).xyz;
    particles[globalIndex].RibbonId = attributes.RibbonId;
    particles[globalIndex].Color = GetAttribute(attributes, ParticleAttribute_Color);
    particles[globalIndex].Metadata = float4(
        float(pc.EmitterIndex),
        asfloat(emissionIndex),
        GetAttribute(attributes, ParticleAttribute_Lifetime).x,
        1.0
    );
}
