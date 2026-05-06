struct ParticleState
{
    float4 PositionSize;    // xyz = position, w = size
    float4 VelocityAge;     // xyz = velocity, w = age
    float4 Tangent;         // xyz = tangent, w = unused
    float4 Color;
    float4 Metadata;        // x = emitter index, y = random seed, z = lifetime, w = alive
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 MetaA; // x = offset in particle buffer, y = max particles, z = module offset(spawn), w = module count(spawn)
    float4 MetaB; // x = module offset(update), y = module count(update), z = buffer cursor, w = spawn count
    float4 MetaC; // x = render mode, y = spawn rate seconds, z = gravity scale
};

[[vk::binding(1, 0)]]
StructuredBuffer<Emitter> emitters;

struct Module
{
    float4 Header; // x = type, y = unused, z = parameter 0 index, w = parameter 1 index
    float4 Data0;
    float4 Data1;
    float4 Data2;
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
    float4 Tangent;
};

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;
    float4 ViewportData;
};

struct PushConstants {
    uint EmitterIndex;
};

[[vk::push_constant]]
PushConstants pc;

float Hash1(float x)
{
    return frac(sin(x * 91.3458 + 12.345) * 45678.5453);
}

float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float4 ResolveValue(int parameterIndex, float4 fallbackValue)
{
    if (parameterIndex < 0)
        return fallbackValue;

    return parameters[parameterIndex].Value;
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
    if (attrId == 6u)
        return table.Tangent;

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
    else if (attrId == 6u)
        table.Tangent = value;
}

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared < 0.000001)
        return fallback;

    return value * rsqrt(lengthSquared);
}

uint SpawnOrderInBatch(uint localIndex, uint start, uint capacity)
{
    return (localIndex + capacity - start) % capacity;
}

/**
 * Determines if a given local index falls within the spawn range defined by start and count,
 * considering wrap-around.
 *
 * @param localIndex The index to check.
 * @param start The starting index of the spawn range.
 * @param count The number of indices in the spawn range.
 * @param capacity The total capacity of the buffer (used for wrap-around).
 * @return true if localIndex is within the spawn range, false otherwise.
 */
bool InSpawnRange(uint localIndex, uint start, uint count, uint capacity)
{
    if (count == 0)
        return false;

    if (count >= capacity)
        return true;

    uint end = start + count;
    if (end <= capacity) {
        return localIndex >= start && localIndex < end;
    }

    uint wrappedEnd = end - capacity;
    return localIndex >= start || localIndex < wrappedEnd;
}

void BuildBasis(float3 axis, out float3 right, out float3 up)
{
    float3 helper = abs(axis.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    right = SafeNormalize(cross(helper, axis), float3(1.0, 0.0, 0.0));
    up = cross(axis, right);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Emitter emitter = emitters[pc.EmitterIndex];
    uint localIndex = dispatchThreadId.x;
    uint emitterCount = (uint)emitter.MetaA.y;
    if (localIndex >= emitterCount)
        return;

    uint spawnCursor = (uint)emitter.MetaB.z;
    uint spawnCount = (uint)emitter.MetaB.w;
    if (!InSpawnRange(localIndex, spawnCursor, spawnCount, emitterCount))
        return;

    uint globalIndex = (uint)(emitter.MetaA.x + localIndex);
    float2 seedBase = float2(float(globalIndex), TimeData.y + float(spawnCursor));

    AttributeTable attributes;
    attributes.Position = float4(0.0, 0.0, 0.0, 0.0);
    attributes.Velocity = float4(0.0, 0.0, 0.0, 0.0);
    attributes.Color = float4(1.0, 1.0, 1.0, 1.0);
    attributes.Size = float4(6.0, 0.0, 0.0, 0.0);
    attributes.Lifetime = float4(1.0, 0.0, 0.0, 0.0);
    attributes.Tangent = float4(1.0, 0.0, 0.0, 0.0);

    uint moduleOffset = (uint)emitter.MetaA.z;
    uint moduleCount = (uint)emitter.MetaA.w;

    for (uint i = 0u; i < moduleCount; ++i)
    {
        Module module = modules[moduleOffset + i];
        uint type = (uint)module.Header.x;
        uint target = (uint)module.Header.y;

        int param0 = (int)module.Header.z;

        if (type == 1u) // SetPositionDisk
        {
            float3 center = module.Data0.xyz;
            float3 normal = module.Data1.xyz;
            float maxRadius = module.Data0.w;

            float radiusRandom = sqrt(Hash2(seedBase + float2(8.63, 6.53)));
            float arcRandom = Hash2(seedBase + float2(4.71, 1.29));

            float spawnAngle = arcRandom * 6.28318530718;
            float radius = maxRadius * radiusRandom;

            float3 right;
            float3 up;
            BuildBasis(normal, right, up);

            float3 direction = right * cos(spawnAngle) + up * sin(spawnAngle);

            float3 position = center + direction * radius;
            SetAttribute(attributes, target, float4(position, 0.0));

            float3 tangent = right * -sin(spawnAngle) + up * cos(spawnAngle);
            tangent = SafeNormalize(tangent, right);
            SetAttribute(attributes, 6u, float4(tangent, 0.0));
        }
        else if (type == 2u) // SetVelocityCone
        {
            float3 axis = SafeNormalize(module.Data0.xyz, float3(0.0, 1.0, 0.0));
            float angle = module.Data0.w * 0.5;

            float angleRandom = Hash2(seedBase + float2(3.17, 9.41));
            float speedRandom = Hash2(seedBase + float2(5.23, 2.19));
            float coneRandom = Hash2(seedBase + float2(8.41, 4.77));

            float speed = lerp(module.Data1.x, module.Data1.y, speedRandom);

            float cosTheta = lerp(1.0, cos(angle), coneRandom);
            float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
            float phi = angleRandom * 6.28318530718;

            float3 right;
            float3 up;
            BuildBasis(axis, right, up);

            float3 direction = right * (cos(phi) * sinTheta) +
                               up * (sin(phi) * sinTheta) +
                               axis * cosTheta;
            direction = SafeNormalize(direction, axis);

            float3 velocity = direction * speed;
            SetAttribute(attributes, target, float4(velocity, 0.0));

            float3 tangent = SafeNormalize(velocity, GetAttribute(attributes, 6u).xyz);
            SetAttribute(attributes, 6u, float4(tangent, 0.0));
        }
        else if (type == 3u) // SetLifetime
        {
            float lifetimeRandom = Hash2(seedBase + float2(1.37, 7.11));
            float lifetime = lerp(module.Data0.x, module.Data0.y, lifetimeRandom);
            SetAttribute(attributes, target, float4(lifetime, 0.0, 0.0, 0.0));
        }
        else if (type == 4u) // SetSize
        {
            float size = module.Data0.x;
            SetAttribute(attributes, target, float4(size, 0.0, 0.0, 0.0));
        }
        else if (type == 5u) // SetColor
        {
            float4 color = ResolveValue(param0, module.Data0);
            SetAttribute(attributes, target, color);
        }
        else if (type == 11u) // SetPositionOnCircle
        {
            float2 center = module.Data0.xy;
            float radius = module.Data0.z;
            float angularSpeed = module.Data0.w;
            float startAngle = module.Data1.x;
            float angle = TimeData.y * angularSpeed + startAngle;

            float2 position = center + float2(cos(angle), sin(angle)) * radius;
            SetAttribute(attributes, target, float4(position, 0.0, 1.0));

            float orientation = angularSpeed < 0.0 ? -1.0 : 1.0;
            float2 tangent = float2(-sin(angle), cos(angle)) * orientation;
            SetAttribute(attributes, 6u, float4(tangent, 0.0, 0.0));

            SetAttribute(attributes, 2u, float4(0.0, 0.0, 0.0, 0.0));
        }
        else if (type == 13u) // SetPositionCircularPath
        {
            float3 baseOffset = module.Data0.xyz;
            float3 primaryAmplitude = module.Data1.xyz;
            float3 secondaryAmplitude = module.Data2.xyz;
            float timeScale = module.Data0.w;

            float spawnRate = max(emitter.MetaC.y, 0.001);
            uint spawnOrder = SpawnOrderInBatch(localIndex, spawnCursor, emitterCount);
            spawnOrder = min(spawnOrder, spawnCount - 1u);
            float timeOffset = float((spawnCount - 1u) - spawnOrder) / spawnRate;
            float sampleTime = TimeData.y - timeOffset;

            float phase = sampleTime * timeScale;

            float x = baseOffset.x + (primaryAmplitude.x * sin(phase)) + (secondaryAmplitude.x * sin(phase * 2.1));
            float y = baseOffset.y + (primaryAmplitude.y * sin((phase * 0.58) + 0.65)) +
                (secondaryAmplitude.y * cos((phase * 1.34) - 0.4));
            float z = baseOffset.z + (primaryAmplitude.z * cos((phase * 0.72) + 0.35)) +
                (secondaryAmplitude.z * sin((phase * 1.61) - 0.2));

            SetAttribute(attributes, target, float4(x, y, z, 1.0));
            SetAttribute(attributes, 2u, float4(0.0, 0.0, 0.0, 0.0));

            float dx = primaryAmplitude.x * cos(phase) + secondaryAmplitude.x * 2.1 * cos(phase * 2.1);
            float dy = primaryAmplitude.y * 0.58 * cos(phase * 0.58 + 0.65) - secondaryAmplitude.y * 1.34 * sin(phase * 1.34 - 0.4);
            float dz = -primaryAmplitude.z * 0.72 * sin((phase * 0.72) + 0.35) + secondaryAmplitude.z * 1.61 * cos((phase * 1.61) - 0.2);
            float3 tangent = SafeNormalize(float3(dx, dy, dz), float3(1.0, 0.0, 0.0));

            SetAttribute(attributes, 6u, float4(tangent, 0.0));
        }
    }

    particles[globalIndex].PositionSize = float4(GetAttribute(attributes, 1u).xyz, GetAttribute(attributes, 4u).x);
    particles[globalIndex].VelocityAge = float4(GetAttribute(attributes, 2u).xyz, 0.0);
    particles[globalIndex].Tangent = float4(GetAttribute(attributes, 6u).xyz, 0.0);
    particles[globalIndex].Color = GetAttribute(attributes, 3u);
    particles[globalIndex].Metadata = float4(float(pc.EmitterIndex), Hash1(float(globalIndex)), GetAttribute(attributes, 5u).x, 1.0);
}
