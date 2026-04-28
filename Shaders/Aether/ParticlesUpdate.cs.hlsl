struct ParticleState
{
    float4 PositionSize;    // xy = position, z = size, w = alive (0 or 1)
    float4 VelocityAge;     // xy = velocity, z = age, w = lifetime
    float4 Color;
    float4 Metadata;        // x = emitter index, y = random seed
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 MetaA; // x = offset in particle buffer, y = emitter count, z = module offset(spawn), w = module count(spawn)
    float4 MetaB; // x = module offset(update), y = module count(update), z = spawn cursor, w = spawn count
    float4 MetaC; // x = render mode
};

[[vk::binding(1, 0)]]
StructuredBuffer<Emitter> emitters;

struct Module
{
    float4 Header; // x = type, y = unused, z = parameter 0 index, w = parameter 1 index
    float4 Data0;
    float4 Data1;
};

[[vk::binding(2, 0)]]
StructuredBuffer<Module> modules;

struct Parameter
{
    float4 Value;
};

[[vk::binding(3, 0)]]
StructuredBuffer<Parameter> parameters;

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;                  // x = delta time, y = total time, z = total particle count, w = max particle count
};

float4 ResolveValue(int parameterIndex, float4 fallbackValue)
{
    if (parameterIndex < 0)
        return fallbackValue;

    return parameters[parameterIndex].Value;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    uint totalParticleCount = (uint)TimeData.z;
    if (particleIndex >= totalParticleCount)
        return;

    ParticleState state = particles[particleIndex];
    if (state.PositionSize.w < 0.5)
    {
        particles[particleIndex].PositionSize.z = 0.0;
        particles[particleIndex].Color.a = 0.0;
        return;
    }

    uint emitterIndex = (uint)(state.Metadata.x + 0.5);
    Emitter emitter = emitters[emitterIndex];
    float dt = TimeData.x;
    bool isRibbon = ((uint)(emitter.MetaC.x + 0.5)) == 1u;

    float2 position = state.PositionSize.xy;
    float2 velocity = state.VelocityAge.xy;
    float4 color = float4(1.0, 1.0, 1.0, 1.0);
    float size = state.PositionSize.z;
    float age = state.VelocityAge.z + dt;
    float lifetime = max(state.VelocityAge.w, 0.001);
    if (isRibbon)
        age = min(age, lifetime * 0.995);
    float life = clamp(age / lifetime, 0.0, 1.0);
    bool kill = !isRibbon && age >= lifetime;

    uint moduleOffset = (uint)emitter.MetaB.x;
    uint moduleCount = (uint)emitter.MetaB.y;

    for (uint i = 0u; i < moduleCount; ++i)
    {
        Module module = modules[moduleOffset + i];
        uint type = (uint)module.Header.x;

        int param0 = (int)module.Header.z;
        int param1 = (int)module.Header.w;

        if (type == 6u) // ApplyGravity
        {
            velocity += module.Data0.xy * module.Data0.z * dt;
        }
        else if (type == 7u) // ApplyLinearDrag
        {
            velocity *= max(0.0, 1.0 - (module.Data0.x * dt));
        }
        else if (type == 8u) // ColorOverLife
        {
            float4 startColor = ResolveValue(param0, module.Data0);
            float4 endColor = ResolveValue(param1, module.Data1);
            color = lerp(startColor, endColor, life);
        }
        else if (type == 9u) // SizeOverLife
        {
            size = lerp(module.Data0.x, module.Data0.y, life);
        }
        else if (type == 10u) // KillOutsideBounds
        {
            bool outsideBounds = position.x < module.Data0.x || position.x > module.Data1.x ||
                                 position.y < module.Data0.y || position.y > module.Data1.y;
            kill = kill || (!isRibbon && outsideBounds);
        }
    }

    position += velocity * dt;

    if (kill)
    {
        state.PositionSize = float4(position, 0.0, 0.0);
        state.VelocityAge = float4(0.0, 0.0, 0.0, 0.0);
        state.Color = float4(0.0, 0.0, 0.0, 0.0);

        particles[particleIndex] = state;
        return;
    }

    state.PositionSize = float4(position, size, 1.0);
    state.VelocityAge = float4(velocity, age, lifetime);
    state.Color = color;

    particles[particleIndex] = state;
}
