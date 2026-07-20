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

struct SystemInstance
{
    uint ParticleBaseOffset;
    uint EmitterBaseOffset;
    uint OpBaseOffset;
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
    float4 RibbonId;
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

float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float4 RandomVector(float2 seed)
{
    return float4(
        Hash2(seed + float2(1.31, 2.17)),
        Hash2(seed + float2(3.11, 4.29)),
        Hash2(seed + float2(5.71, 6.13)),
        Hash2(seed + float2(7.43, 8.59))
    );
}

float4 ResolveValue(int parameterIndex, float4 fallbackValue)
{
    if (parameterIndex < 0)
        return fallbackValue;

    return parameters[parameterIndex].Value;
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

struct VortexRibbonPathParams
{
    float3 Center;
    float  OrbitSpeed;
    float  BaseRadius;
    float  RadiusAmplitude;
    float  RadiusSpeed;
    float  PulseAmplitude;
    float  PulseSpeed;
    float  PulsePhase;
    float  CurlAmplitude;
    float2 CurlFrequency;
    float2 CurlPhase;
    float  DepthAmplitude;
    float  DepthFrequency;
    float  DepthPhase;
};

float3 VortexRibbonPath(float timeSeconds, VortexRibbonPathParams p)
{
    float phase = timeSeconds * p.OrbitSpeed;

    float radialPulse = p.PulseAmplitude * sin((timeSeconds * p.PulseSpeed) + p.PulsePhase);

    float radius =
        p.BaseRadius +
        (p.RadiusAmplitude * (0.5 + 0.5 * sin(timeSeconds * p.RadiusSpeed))) +
        radialPulse;

    float2 swirl = float2(cos(phase), sin(phase * 0.96)) * radius;

    float2 curl = float2(
        cos(phase * p.CurlFrequency.x + p.CurlPhase.x),
        sin(phase * p.CurlFrequency.y + p.CurlPhase.y)
    ) * p.CurlAmplitude;

    float z = p.Center.z + p.DepthAmplitude * sin(phase * p.DepthFrequency + p.DepthPhase);

    return float3(p.Center.xy + swirl + curl, z);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    SystemInstance instance = instances[pc.InstanceIndex];

    uint emitterIndex = instance.EmitterBaseOffset + pc.EmitterIndex;
    Emitter emitter = emitters[emitterIndex];

    uint localIndex = dispatchThreadId.x;
    uint emitterCount = (uint)emitter.MetaA.y;
    if (localIndex >= emitterCount)
        return;

    SpawnRequest request = spawnRequests[instance.SpawnRequestBaseOffset + pc.EmitterIndex];
    if (request.Generation != instance.Generation)
        return;

    uint spawnCursor = request.SpawnCursor;
    uint spawnCount = request.SpawnCount;

    if (!InSpawnRange(localIndex, spawnCursor, spawnCount, emitterCount))
        return;

    uint globalIndex = instance.ParticleBaseOffset + (uint)emitter.MetaA.x + localIndex;
    float2 seedBase = float2((float)globalIndex, TimeData.y + float(spawnCursor));
    float particleSeed = Hash1((float)globalIndex);
    float randomInput = Hash2(seedBase + float2(8.11, 3.41));
    uint spawnOrder = SpawnOrderInBatch(localIndex, spawnCursor, emitterCount);
    spawnOrder = min(spawnOrder, spawnCount - 1u);
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
    attributes.RibbonId = 0.0;
    attributes.Temp0 = 0.0;
    attributes.Temp1 = 0.0;
    attributes.Temp2 = 0.0;
    attributes.Temp3 = 0.0;

    uint opOffset = (uint)emitter.MetaA.z;
    uint opCount = (uint)emitter.MetaA.w;

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
        else if (type == 1u) // RandomRange
        {
            float2 seed = seedBase + float2(float(i) * 1.7, float(target) * 2.3);
            float4 randomValue = RandomVector(seed);
            float4 value0 = ResolveValue(param0, op.Data0);
            float4 value1 = ResolveValue(param1, op.Data1);
            float4 value = lerp(value0, value1, randomValue);
            SetAttribute(attributes, target, value);
        }
        else if (type == 2u) // SampleDisk
        {
            float3 center = op.Data0.xyz;
            float3 normal = op.Data1.xyz;
            float maxRadius = op.Data0.w;

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
            SetAttribute(attributes, 8u, float4(tangent, 0.0));
        }
        else if (type == 3u) // SampleCone
        {
            float3 axis = SafeNormalize(op.Data0.xyz, float3(0.0, 1.0, 0.0));
            float angle = op.Data0.w * 0.5;

            float angleRandom = Hash2(seedBase + float2(3.17, 9.41));
            float speedRandom = Hash2(seedBase + float2(5.23, 2.19));
            float coneRandom = Hash2(seedBase + float2(8.41, 4.77));

            float speed = lerp(op.Data1.x, op.Data1.y, speedRandom);

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
            SetAttribute(attributes, 8u, float4(tangent, 0.0));
        }
        else if (type == 4u) // SampleBox
        {
            float3 minBounds = op.Data0.xyz;
            float3 maxBounds = op.Data1.xyz;

            float2 seed = seedBase + float2(float(i) * 1.3, 5.7);
            float3 random = RandomVector(seed).xyz;

            float3 position = lerp(minBounds, maxBounds, random);
            SetAttribute(attributes, target, float4(position, 0.0));
        }
        else if (type == 10u) // SetPositionOnCircle
        {
            float3 center = op.Data0.xyz;
            float radius = op.Data0.w;
            float angularSpeed = op.Data1.x;
            float startAngle = op.Data1.y;
            float angle = TimeData.y * angularSpeed + startAngle;

            float3 position = center + float3(cos(angle), sin(angle), 0.0) * radius;
            SetAttribute(attributes, target, float4(position, 1.0));

            float orientation = angularSpeed < 0.0 ? -1.0 : 1.0;
            float3 tangent = float3(-sin(angle), cos(angle), 0.0) * orientation;
            SetAttribute(attributes, 8u, float4(tangent, 0.0));

            SetAttribute(attributes, 4u, float4(0.0, 0.0, 0.0, 0.0));
        }
        else if (type == 11u) // SetPositionCircularPath
        {
            float3 baseOffset = op.Data0.xyz;
            float3 primaryAmplitude = op.Data1.xyz;
            float3 secondaryAmplitude = op.Data2.xyz;
            float timeScale = op.Data0.w;

            float spawnRate = max(emitter.MetaC.y, 0.001);
            float timeOffset = float((spawnCount - 1u) - spawnOrder) / spawnRate;
            float sampleTime = TimeData.y - timeOffset;

            float phase = sampleTime * timeScale;

            float x = baseOffset.x + (primaryAmplitude.x * sin(phase)) + (secondaryAmplitude.x * sin(phase * 2.1));
            float y = baseOffset.y + (primaryAmplitude.y * sin((phase * 0.58) + 0.65)) +
                (secondaryAmplitude.y * cos((phase * 1.34) - 0.4));
            float z = baseOffset.z + (primaryAmplitude.z * cos((phase * 0.72) + 0.35)) +
                (secondaryAmplitude.z * sin((phase * 1.61) - 0.2));

            SetAttribute(attributes, target, float4(x, y, z, 1.0));
            SetAttribute(attributes, 4u, 0.0);

            float dx = primaryAmplitude.x * cos(phase) + secondaryAmplitude.x * 2.1 * cos(phase * 2.1);
            float dy = primaryAmplitude.y * 0.58 * cos(phase * 0.58 + 0.65) - secondaryAmplitude.y * 1.34 * sin(phase * 1.34 - 0.4);
            float dz = -primaryAmplitude.z * 0.72 * sin((phase * 0.72) + 0.35) + secondaryAmplitude.z * 1.61 * cos((phase * 1.61) - 0.2);
            float3 tangent = SafeNormalize(float3(dx, dy, dz), float3(1.0, 0.0, 0.0));

            SetAttribute(attributes, 8u, float4(tangent, 0.0));
        }
        else if (type == 12u) // SetPositionVortexRibbonPath
        {
            VortexRibbonPathParams params;
            params.Center = op.Data0.xyz;

            params.OrbitSpeed = op.Data1.x;
            params.BaseRadius = op.Data1.y;
            params.RadiusAmplitude = op.Data1.z;
            params.RadiusSpeed = op.Data1.w;

            params.PulseAmplitude = op.Data2.x;
            params.PulseSpeed = op.Data2.y;
            params.PulsePhase = 0.0;
            params.CurlAmplitude = op.Data2.z;
            params.DepthAmplitude = op.Data2.w;

            params.CurlFrequency = float2(1.9, 1.7);
            params.CurlPhase = float2(0.6, -0.3);
            params.DepthFrequency = 0.73;
            params.DepthPhase = 0.4;

            float spawnRate = max(emitter.MetaC.y, 0.001);
            float timeOffset = float((spawnCount - 1u) - spawnOrder) / spawnRate;
            float sampleTime = TimeData.y - timeOffset;

            float3 position = VortexRibbonPath(sampleTime, params);
            SetAttribute(attributes, target, float4(position.xyz, 1.0));
            SetAttribute(attributes, 4u, 0.0);

            float tangentStep = max(1.0 / spawnRate, 0.001);
            float3 prevPosition = VortexRibbonPath(sampleTime - tangentStep, params);
            float3 nextPosition = VortexRibbonPath(sampleTime + tangentStep, params);
            float3 tangent = SafeNormalize(nextPosition - prevPosition, float3(1.0, 0.0, 0.0));

            SetAttribute(attributes, 8u, float4(tangent, 0.0));
        }
        else if (type == 13u) // SetRibbonIdFromSpawnOrder
        {
            uint ribbonCount = max(1u, (uint)op.Data0.x);
            uint firstRibbonId = (uint)op.Data0.y;
            uint ribbonId = firstRibbonId + (emissionIndex % ribbonCount);
            SetAttribute(attributes, target, float4(float(ribbonId), 0.0, 0.0, 0.0));
        }
        else if (type == 14u) // SampleCurve
        {
            float inputValue = ResolveDynamicInput(uint(op.Data0.x + 0.5), randomInput, particleSeed);
            float value = SampleCurve(param0, inputValue);
            SetAttribute(attributes, target, float4(value, 0.0, 0.0, 0.0));
        }
        else if (type == 19u) // CopyFromAttribute
        {
            float4 value = GetAttribute(attributes, uint(op.Data0.x + 0.5));
            SetAttribute(attributes, target, value);
        }
    }

    particles[globalIndex].PositionSize = float4(GetAttribute(attributes, 1u).xyz, GetAttribute(attributes, 6u).x);
    particles[globalIndex].VelocityAge = float4(GetAttribute(attributes, 4u).xyz, 0.0);
    particles[globalIndex].Transform = float4(GetAttribute(attributes, 2u).x, GetAttribute(attributes, 3u).x, 0.0, 0.0);
    particles[globalIndex].TangentRibbonId = float4(GetAttribute(attributes, 8u).xyz, GetAttribute(attributes, 9u).x);
    particles[globalIndex].Color = GetAttribute(attributes, 5u);
    particles[globalIndex].Metadata = float4(
        float(pc.EmitterIndex),
        asfloat(emissionIndex),
        GetAttribute(attributes, 7u).x,
        1.0
    );
}
