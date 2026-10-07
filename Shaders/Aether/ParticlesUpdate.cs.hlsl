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

#include "ParticleAttributes.hlsl"

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;     // x = delta time, y = total time, z = total particle count, w = max particle count
    float4 ViewportData; // x = viewport width, y = viewport height, z = unused, w = unused
};

struct PushConstants {
    uint InstanceIndex;
};

[[vk::push_constant]]
PushConstants pc;

float Hash1(float x) {
  return frac(sin(x * 91.3458 + 12.345) * 45678.5453);
}

float ResolveDynamicInput(uint inputType, float normalizedAge, float particleSeed)
{
    if (inputType == 1u) // DeltaTime
        return TimeData.x;

    if (inputType == 2u) // NormalizedAge
        return normalizedAge;

    if (inputType == 3u) // EmitterTime
        return TimeData.y;

    if (inputType == 4u) // Random
        return frac(sin((particleSeed * 91.37) + TimeData.y * 0.71) * 43758.5453);

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

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SystemInstance instance = instances[pc.InstanceIndex];

    uint localParticleIndex = dispatchThreadId.x;
    if (localParticleIndex >= instance.ParticleCount)
        return;

    uint particleIndex = instance.ParticleBaseOffset + localParticleIndex;

    ParticleState state = particles[particleIndex];
    if (state.Metadata.w < 0.5)
    {
        particles[particleIndex].PositionSize.w = 0.0;
        particles[particleIndex].Color.a = 0.0;
        return;
    }

    float particleSeed = Hash1((float)particleIndex);

    uint emitterIndex = instance.EmitterBaseOffset + (uint)(state.Metadata.x + 0.5);
    Emitter emitter = emitters[emitterIndex];
    float dt = TimeData.x;

    AttributeTable attributes = LoadAttributes(state);
    float age = state.VelocityAge.w + dt;
    float lifetime = max(state.Metadata.z, 0.001);
    float life = clamp(age / lifetime, 0.0, 1.0);
    bool kill = age >= lifetime;

    uint operationOffset = (uint)emitter.MetaB.x;
    uint operationCount = (uint)emitter.MetaB.y;

    SOperationExecutor executor = CreateOperationExecutor(operands, constants, parameters);

    SOperationExecutorContext context = (SOperationExecutorContext)0;
    context.Instance = instance;
    context.InputContext = life;
    context.ParticleSeed = particleSeed;
    context.Attributes = attributes;
    context.DeltaTime = dt;
    context.NormalizedAge = life;
    context.Time = TimeData.y;
    context.Kill = kill;

    for (uint i = 0u; i < operationCount; ++i)
    {
        context.OperationIndex = i;
        SGPUOperation operation = operations[operationOffset + i];

        executor.Execute(operation, context);
    }

    attributes = context.Attributes;
    kill = context.Kill;

    float4 position = GetAttribute(attributes, ParticleAttribute_Position);
    float4 velocity = GetAttribute(attributes, ParticleAttribute_Velocity);

    position.xyz += velocity.xyz * dt;
    SetAttribute(attributes, ParticleAttribute_Position, position);

    if (kill)
    {
        state.PositionSize = float4(position.xyz, 0.0);
        state.VelocityAge = float4(0.0, 0.0, 0.0, 0.0);
        state.Transform = float4(0.0, 0.0, 0.0, 0.0);
        state.Color = float4(0.0, 0.0, 0.0, 0.0);
        state.Metadata.w = 0.0;

        particles[particleIndex] = state;
        return;
    }

    particles[particleIndex] = StoreAttributes(state, attributes, age);
}
