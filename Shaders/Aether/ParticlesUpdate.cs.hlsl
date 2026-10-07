#include "ParticleData.hlsl"
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

[[vk::binding(0, 0)]]
RWStructuredBuffer<SGPUParticle> particles;

[[vk::binding(1, 0)]]
StructuredBuffer<SGPUEmitter> emitters;

[[vk::binding(3, 0)]]
StructuredBuffer<SGPUParameter> parameters;

[[vk::binding(4, 0)]]
StructuredBuffer<SGPUSystemInstance> instances;

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
    SGPUSystemInstance instance = instances[pc.InstanceIndex];

    uint localParticleIndex = dispatchThreadId.x;
    if (localParticleIndex >= instance.ParticleCount)
        return;

    uint particleIndex = instance.ParticleBaseOffset + localParticleIndex;

    SGPUParticle state = particles[particleIndex];
    if (state.Alive < 0.5)
    {
        particles[particleIndex].Size = 0.0;
        particles[particleIndex].Color.a = 0.0;
        return;
    }

    float particleSeed = Hash1((float)particleIndex);

    uint emitterIndex = instance.EmitterBaseOffset + state.EmitterIndex;
    SGPUEmitter emitter = emitters[emitterIndex];
    float dt = TimeData.x;

    AttributeTable attributes = LoadAttributes(state);
    float age = state.Age + dt;
    float lifetime = max(state.Lifetime, 0.001);
    float life = clamp(age / lifetime, 0.0, 1.0);
    bool kill = age >= lifetime;

    uint operationOffset = emitter.UpdateOperationOffset;
    uint operationCount = emitter.UpdateOperationCount;

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
        state.Position = position.xyz;
        state.Size = 0.0;
        state.Velocity = 0.0;
        state.Age = 0.0;
        state.Transform = float4(0.0, 0.0, 0.0, 0.0);
        state.Color = float4(0.0, 0.0, 0.0, 0.0);
        state.Alive = 0.0;

        particles[particleIndex] = state;
        return;
    }

    particles[particleIndex] = StoreAttributes(state, attributes, age);
}
