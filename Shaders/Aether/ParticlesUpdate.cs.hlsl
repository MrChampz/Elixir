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
    float4 MetaC; //
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

struct AttributeTable
{
    float4 Position;
    float4 Velocity;
    float4 Color;
    float4 Size;
    float4 Lifetime;
};

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;     // x = delta time, y = total time, z = total particle count, w = max particle count
    float4 ViewportData; // x = viewport width, y = viewport height, z = unused, w = unused
};

float4 ResolveValue(int parameterIndex, float4 fallbackValue)
{
    if (parameterIndex < 0)
        return fallbackValue;

    return parameters[parameterIndex].Value;
}

AttributeTable LoadAttributes(ParticleState state)
{
    AttributeTable table;
    table.Position = float4(state.PositionSize.xy, 0.0, state.PositionSize.w);
    table.Velocity = float4(state.VelocityAge.xy, 0.0, 0.0);
    table.Color = state.Color;
    table.Size = float4(state.PositionSize.z, 0.0, 0.0, 0.0);
    table.Lifetime = float4(state.VelocityAge.w, state.VelocityAge.z, 0.0, 0.0);
    return table;
}

ParticleState StoreAttributes(ParticleState state, AttributeTable table, float age)
{
    state.PositionSize = float4(table.Position.xy, table.Size.x, table.Position.w);
    state.VelocityAge = float4(table.Velocity.xy, age, table.Lifetime.x);
    state.Color = table.Color;
    return state;
}

float4 GetAttribute(AttributeTable table, uint attrId)
{
    if (attrId == 1u)
        return table.Position;
    if (attrId == 2u)
        return table.Velocity;
    if (attrId == 3u)
        return table.Color;
    if (attrId == 4u)
        return table.Size;
    if (attrId == 5u)
        return table.Lifetime;

    return float4(0.0, 0.0, 0.0, 0.0);
}

void SetAttribute(inout AttributeTable table, uint attrId, float4 value)
{
    if (attrId == 1u)
        table.Position = value;
    else if (attrId == 2u)
        table.Velocity = value;
    else if (attrId == 3u)
        table.Color = value;
    else if (attrId == 4u)
        table.Size = value;
    else if (attrId == 5u)
        table.Lifetime = value;
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
    uint renderMode = (uint)(emitter.MetaC.x + 0.5);

    AttributeTable attributes = LoadAttributes(state);
    float age = state.VelocityAge.z + dt;
    float lifetime = max(state.VelocityAge.w, 0.0001);
    float life = clamp(age / lifetime, 0.0, 1.0);
    bool kill = age >= lifetime;

    uint moduleOffset = (uint)emitter.MetaB.x;
    uint moduleCount = (uint)emitter.MetaB.y;

    for (uint i = 0u; i < moduleCount; ++i)
    {
        Module module = modules[moduleOffset + i];
        uint type = (uint)module.Header.x;
        uint target = (uint)module.Header.y;

        int param0 = (int)module.Header.z;
        int param1 = (int)module.Header.w;

        if (type == 6u) // ApplyGravity
        {
            float4 value = GetAttribute(attributes, target);
            value.xy += module.Data0.xy * module.Data0.z * dt;
            SetAttribute(attributes, target, value);
        }
        else if (type == 7u) // ApplyLinearDrag
        {
            float4 value = GetAttribute(attributes, target);
            value.xy *= max(0.0, 1.0 - (module.Data0.x * dt));
            SetAttribute(attributes, target, value);
        }
        else if (type == 8u) // ColorOverLife
        {
            float4 startColor = ResolveValue(param0, module.Data0);
            float4 endColor = ResolveValue(param1, module.Data1);
            float4 color = lerp(startColor, endColor, life);
            SetAttribute(attributes, target, color);
        }
        else if (type == 9u) // SizeOverLife
        {
            float size = lerp(module.Data0.x, module.Data0.y, life);
            SetAttribute(attributes, target, float4(size, 0.0, 0.0, 0.0));
        }
        else if (type == 10u) // KillOutsideBounds
        {
            float4 position = GetAttribute(attributes, 1u);
            bool outsideBounds = position.x < module.Data0.x || position.x > module.Data1.x ||
                                 position.y < module.Data0.y || position.y > module.Data1.y;
            kill = kill || outsideBounds;
        }
    }

    float4 position = GetAttribute(attributes, 1u);
    float4 velocity = GetAttribute(attributes, 2u);

    if (renderMode != 1u) // Non-ribbon particles are affected by velocity in a simple Euler manner
    {
        position.xy += velocity.xy * dt;
        SetAttribute(attributes, 1u, position);
    }

    if (kill)
    {
        state.PositionSize = float4(position.xy, 0.0, 0.0);
        state.VelocityAge = float4(0.0, 0.0, 0.0, 0.0);
        state.Color = float4(0.0, 0.0, 0.0, 0.0);

        particles[particleIndex] = state;
        return;
    }

    particles[particleIndex] = StoreAttributes(state, attributes, age);
}