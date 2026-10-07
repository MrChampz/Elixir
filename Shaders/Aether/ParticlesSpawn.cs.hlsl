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

struct AttributeTable
{
    float4 Position;
    float4 Rotation;
    float4 Scale;
    float4 Velocity;
    float4 Color;
    float4 Size;
    float4 Lifetime;
    float4 Tangent;
    uint   RibbonId;
    float4 Temp0;
    float4 Temp1;
    float4 Temp2;
    float4 Temp3;
};

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



float4 GetAttribute(AttributeTable table, uint attrId)
{
    if (attrId == 1u)
        return table.Position;
    if (attrId == 2u)
        return table.Rotation;
    if (attrId == 3u)
        return table.Scale;
    if (attrId == 4u)
        return table.Velocity;
    if (attrId == 5u)
        return table.Color;
    if (attrId == 6u)
        return table.Size;
    if (attrId == 7u)
        return table.Lifetime;
    if (attrId == 8u)
        return table.Tangent;
    if (attrId == 9u)
        return float4(float(table.RibbonId), 0.0, 0.0, 0.0);
    if (attrId == 10u)
        return table.Temp0;
    if (attrId == 11u)
        return table.Temp1;
    if (attrId == 12u)
        return table.Temp2;
    if (attrId == 13u)
        return table.Temp3;

    return float4(0.0, 0.0, 0.0, 0.0);
}

void SetAttribute(inout AttributeTable table, uint attrId, float4 value)
{
    if (attrId == 1u)
        table.Position = value;
    else if (attrId == 2u)
        table.Rotation = value;
    else if (attrId == 3u)
        table.Scale = value;
    else if (attrId == 4u)
        table.Velocity = value;
    else if (attrId == 5u)
        table.Color = value;
    else if (attrId == 6u)
        table.Size = value;
    else if (attrId == 7u)
        table.Lifetime = value;
    else if (attrId == 8u)
        table.Tangent = value;
    else if (attrId == 10u)
        table.Temp0 = value;
    else if (attrId == 11u)
        table.Temp1 = value;
    else if (attrId == 12u)
        table.Temp2 = value;
    else if (attrId == 13u)
        table.Temp3 = value;
}

// Writes an unsigned integer destination without converting it to float.
void SetAttribute(inout AttributeTable table, uint attrId, uint value)
{
    if (attrId == 9u)
        table.RibbonId = value;
}





#include "Operation.hlsl"

[[vk::binding(2, 0)]]
StructuredBuffer<SOperation> operations;

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

    AttributeTable attributes;
    attributes.Position = 0.0;
    attributes.Rotation = 0.0;
    attributes.Scale = float4(1.0, 0.0, 0.0, 0.0);
    attributes.Velocity = 0.0;
    attributes.Color = 1.0;
    attributes.Size = float4(6.0, 0.0, 0.0, 0.0);
    attributes.Lifetime = float4(1.0, 0.0, 0.0, 0.0);
    attributes.Tangent = float4(1.0, 0.0, 0.0, 0.0);
    attributes.RibbonId = 0u;
    attributes.Temp0 = 0.0;
    attributes.Temp1 = 0.0;
    attributes.Temp2 = 0.0;
    attributes.Temp3 = 0.0;

    uint operationOffset = (uint)emitter.MetaA.z;
    uint operationCount = (uint)emitter.MetaA.w;

    SOperationExecutorContext context = CreateOperationExecutorContext(
        operands, constants, parameters, instance, attributes, randomInput, particleSeed
    );
    context.Time = TimeData.y;
    context.SeedBase = seedBase;
    context.SpawnRate = emitter.MetaC.y;
    context.SpawnCount = spawnCount;
    context.SpawnOrder = spawnOrder;
    context.EmissionIndex = emissionIndex;

    for (uint i = 0u; i < operationCount; ++i)
    {
        context.OperationIndex = i;
        SOperation operation = operations[operationOffset + i];

        SOperationExecutor executor = CreateOperationExecutor(operation, context);
        executor.Execute(context);
    }

    attributes = context.Attributes;

    particles[globalIndex].PositionSize = float4(GetAttribute(attributes, 1u).xyz, GetAttribute(attributes, 6u).x);
    particles[globalIndex].VelocityAge = float4(GetAttribute(attributes, 4u).xyz, 0.0);
    particles[globalIndex].Transform = float4(GetAttribute(attributes, 2u).x, GetAttribute(attributes, 3u).x, 0.0, 0.0);
    particles[globalIndex].Tangent = GetAttribute(attributes, 8u).xyz;
    particles[globalIndex].RibbonId = attributes.RibbonId;
    particles[globalIndex].Color = GetAttribute(attributes, 5u);
    particles[globalIndex].Metadata = float4(
        float(pc.EmitterIndex),
        asfloat(emissionIndex),
        GetAttribute(attributes, 7u).x,
        1.0
    );
}
