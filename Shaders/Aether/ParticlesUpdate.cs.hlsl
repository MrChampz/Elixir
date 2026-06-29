struct ParticleState
{
    float4 PositionSize;    // xyz = position, w = size
    float4 VelocityAge;     // xyz = velocity, w = age
    float4 Transform;       // x = rotation, y = scale
    float4 TangentRibbonId; // xyz = tangent, w = ribbon id
    float4 Color;
    float4 Metadata;        // x = emitter index, y = ribbon link order, z = lifetime, w = alive
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 MetaA; // x = offset in particle buffer, y = max particles, z = op offset(spawn), w = op count(spawn)
    float4 MetaB; // x = op offset(update), y = op count(update), z = buffer cursor, w = spawn count
    float4 MetaC; // x = render mode, y = spawn rate seconds, z = gravity scale, w = next buffer cursor
    float4 MetaD; // x = emission index
};

[[vk::binding(1, 0)]]
StructuredBuffer<Emitter> emitters;

struct Op
{
    float4 Header; // x = type, y = unused, z = parameter 0 index, w = parameter 1 index
    float4 Data0;
    float4 Data1;
    float4 Data2;
};

[[vk::binding(2, 0)]]
StructuredBuffer<Op> ops;

struct Parameter
{
    float4 Value;
};

[[vk::binding(3, 0)]]
StructuredBuffer<Parameter> parameters;

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
    float4 RibbonId;
    float4 Temp0;
    float4 Temp1;
    float4 Temp2;
    float4 Temp3;
};

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;     // x = delta time, y = total time, z = total particle count, w = max particle count
    float4 ViewportData; // x = viewport width, y = viewport height, z = unused, w = unused
};

float Hash1(float x) {
  return frac(sin(x * 91.3458 + 12.345) * 45678.5453);
}

float4 ResolveValue(int parameterIndex, float4 fallbackValue)
{
    if (parameterIndex < 0)
        return fallbackValue;

    return parameters[parameterIndex].Value;
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

float SampleCurve(int baseParameterIndex, float t)
{
    if (baseParameterIndex < 0)
        return 0.0;

    float4 a = parameters[baseParameterIndex].Value;
    float4 b = parameters[baseParameterIndex + 1].Value;

    float samples[8] = { a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w };

    float clampedT = clamp(t, 0.0, 1.0);
    float samplePosition = clampedT * 7.0;

    uint lowerIndex = (uint)floor(samplePosition);
    uint upperIndex = min(lowerIndex + 1u, 7u);
    float fraction = frac(samplePosition);

    return lerp(samples[lowerIndex], samples[upperIndex], fraction);
}

float4 SampleColorCurve(int baseParameterIndex, float t)
{
    if (baseParameterIndex < 0)
        return 1.0;

    float4 samples[8] =
    {
        parameters[baseParameterIndex + 0].Value,
        parameters[baseParameterIndex + 1].Value,
        parameters[baseParameterIndex + 2].Value,
        parameters[baseParameterIndex + 3].Value,
        parameters[baseParameterIndex + 4].Value,
        parameters[baseParameterIndex + 5].Value,
        parameters[baseParameterIndex + 6].Value,
        parameters[baseParameterIndex + 7].Value
    };

    float clampedT = clamp(t, 0.0, 1.0);
    float samplePosition = clampedT * 7.0;

    uint lowerIndex = (uint)floor(samplePosition);
    uint upperIndex = min(lowerIndex + 1u, 7u);
    float fraction = frac(samplePosition);

    return lerp(samples[lowerIndex], samples[upperIndex], fraction);
}

AttributeTable LoadAttributes(ParticleState state)
{
    AttributeTable table;
    table.Position = float4(state.PositionSize.xyz, state.Metadata.w);
    table.Rotation = float4(state.Transform.x, 0.0, 0.0, 0.0);
    table.Scale = float4(state.Transform.y, 0.0, 0.0, 0.0);
    table.Velocity = float4(state.VelocityAge.xyz, 0.0);
    table.Tangent = float4(state.TangentRibbonId.xyz, 0.0);
    table.Color = state.Color;
    table.Size = float4(state.PositionSize.w, 0.0, 0.0, 0.0);
    table.Lifetime = float4(state.Metadata.z, state.VelocityAge.w, 0.0, 0.0);
    table.RibbonId = float4(state.TangentRibbonId.w, 0.0, 0.0, 0.0);
    table.Temp0 = 0.0;
    table.Temp1 = 0.0;
    table.Temp2 = 0.0;
    table.Temp3 = 0.0;

    return table;
}

ParticleState StoreAttributes(ParticleState state, AttributeTable table, float age)
{
    state.PositionSize = float4(table.Position.xyz, table.Size.x);
    state.VelocityAge = float4(table.Velocity.xyz, age);
    state.Transform = float4(table.Rotation.x, table.Scale.x, 0.0, 0.0);
    state.TangentRibbonId = float4(table.Tangent.xyz, table.RibbonId.x);
    state.Color = table.Color;
    state.Metadata = float4(state.Metadata.xy, table.Lifetime.x, table.Position.w);
    return state;
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
        return table.RibbonId;
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
    else if (attrId == 9u)
        table.RibbonId = value;
    else if (attrId == 10u)
        table.Temp0 = value;
    else if (attrId == 11u)
        table.Temp1 = value;
    else if (attrId == 12u)
        table.Temp2 = value;
    else if (attrId == 13u)
        table.Temp3 = value;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    uint totalParticleCount = (uint)TimeData.z;
    if (particleIndex >= totalParticleCount)
        return;

    ParticleState state = particles[particleIndex];
    if (state.Metadata.w < 0.5)
    {
        particles[particleIndex].PositionSize.w = 0.0;
        particles[particleIndex].Color.a = 0.0;
        return;
    }

    float particleSeed = Hash1((float)particleIndex);

    uint emitterIndex = (uint)(state.Metadata.x + 0.5);
    Emitter emitter = emitters[emitterIndex];
    float dt = TimeData.x;

    AttributeTable attributes = LoadAttributes(state);
    float age = state.VelocityAge.w + dt;
    float lifetime = max(state.Metadata.z, 0.001);
    float life = clamp(age / lifetime, 0.0, 1.0);
    bool kill = age >= lifetime;

    uint opOffset = (uint)emitter.MetaB.x;
    uint opCount = (uint)emitter.MetaB.y;

    for (uint i = 0u; i < opCount; ++i)
    {
        Op op = ops[opOffset + i];
        uint type = (uint)op.Header.x;
        uint target = (uint)op.Header.y;

        int param0 = (int)op.Header.z;
        int param1 = (int)op.Header.w;

        if (type == 0u) // SetLiteral
        {
            SetAttribute(attributes, target, ResolveValue(param0, op.Data0));
        }
        else if (type == 5u) // AddWithDelta
        {
            float4 value = GetAttribute(attributes, target);
            float inputMultiplier = ResolveDynamicInput(uint(op.Data1.x + 0.5), life, particleSeed);

            value += ResolveValue(param0, op.Data0) * (dt * inputMultiplier);

            SetAttribute(attributes, target, value);
        }
        else if (type == 6u) // Dampen
        {
            float4 value = GetAttribute(attributes, target);
            float drag = ResolveValue(param0, op.Data0).x;
            value *= max(0.0, 1.0 - drag * dt);
            SetAttribute(attributes, target, value);
        }
        else if (type == 7u) // LerpOverLife
        {
            float4 value0 = ResolveValue(param0, op.Data0);
            float4 value1 = ResolveValue(param1, op.Data1);
            float4 value = lerp(value0, value1, life);
            SetAttribute(attributes, target, value);
        }
        else if (type == 8u) // KillOutsideBounds
        {
            float4 position = GetAttribute(attributes, 1u);
            bool outsideBounds = position.x < op.Data0.x || position.x > op.Data1.x ||
                                 position.y < op.Data0.y || position.y > op.Data1.y ||
                                 position.z < op.Data0.z || position.z > op.Data1.z;
            kill = kill || outsideBounds;
        }
        else if (type == 9u) // AddFromAttribute
        {
            float4 value = GetAttribute(attributes, target);
            float4 sourceValue = GetAttribute(attributes, uint(op.Data0.x + 0.5));
            value += sourceValue * (op.Data0.y * dt);
            SetAttribute(attributes, target, value);
        }
        else if (type == 13u) // SampleCurve
        {
            float inputValue = ResolveDynamicInput(uint(op.Data0.x + 0.5), life, particleSeed);
            float value = SampleCurve(param0, inputValue);
            SetAttribute(attributes, target, float4(value, 0.0, 0.0, 0.0));
        }
        else if (type == 14u) // SampleColorCurve
        {
            float inputValue = ResolveDynamicInput(uint(op.Data0.x + 0.5), life, particleSeed);
            float4 value = SampleColorCurve(param0, inputValue);
            SetAttribute(attributes, target, value);
        }
        else if (type == 15u) // Add
        {
            float4 value = GetAttribute(attributes, target);
            value += ResolveValue(param0, op.Data0);
            SetAttribute(attributes, target, value);
        }
        else if (type == 16u) // Mul
        {
            float4 value = GetAttribute(attributes, target);
            value *= ResolveValue(param0, op.Data0);
            SetAttribute(attributes, target, value);
        }
        else if (type == 17u) // Clamp
        {
            float4 value = GetAttribute(attributes, target);
            float4 minValue = ResolveValue(param0, op.Data0);
            float4 maxValue = ResolveValue(param1, op.Data1);
            value = clamp(value, minValue, maxValue);
            SetAttribute(attributes, target, value);
        }
        else if (type == 18u) // CopyFromAttribute
        {
            float4 value = GetAttribute(attributes, uint(op.Data0.x + 0.5));
            SetAttribute(attributes, target, value);
        }
    }

    float4 position = GetAttribute(attributes, 1u);
    float4 velocity = GetAttribute(attributes, 4u);

    position.xyz += velocity.xyz * dt;
    SetAttribute(attributes, 1u, position);

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
