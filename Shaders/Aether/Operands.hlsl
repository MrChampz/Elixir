#ifndef AETHER_OPERANDS_HLSL
#define AETHER_OPERANDS_HLSL

static const uint OperandSource_Constant           = 0;
static const uint OperandSource_Parameter          = 1;
static const uint OperandSource_DynamicInput       = 2;
static const uint OperandSource_ParticleAttribute  = 3;
static const uint OperandSource_ImmediateUInt      = 4;
static const uint OperandSource_Curve              = 5;
static const uint OperandSource_ConstantVector3    = 6;
static const uint OperandSource_ConstantVector4    = 7;

/** @brief Reserves three tag bits and 29 payload bits in each operand reference. */
static const uint OperandIndexMask = 0x1fffffffu;

/** @brief Reads operation operands from a snapshot taken before destination writes. */
struct SOperandReader
{
    StructuredBuffer<uint> Operands;
    ByteAddressBuffer Constants;
    StructuredBuffer<Parameter> Parameters;

    SOperation Operation;
    SystemInstance Instance;
    AttributeTable Attributes;
    float InputContext;
    float ParticleSeed;

    // Predecode the common numeric positions before selecting the operation.
    float4 CachedValues[2];

    // Returns the packed source reference for an argument.
    uint GetOperandReference(uint argument)
    {
        return Operands[Instance.OperandBaseOffset + Operation.OperandOffset + argument];
    }

    // Decodes a numeric value without consulting the cache.
    float4 DecodeValue(uint argument)
    {
        uint reference = GetOperandReference(argument);

        uint source = reference >> 29;
        uint index = reference & OperandIndexMask;
        uint address = (Instance.ConstantBaseOffset + index) * 4u;

        if (source == OperandSource_Constant)
            return float4(asfloat(Constants.Load(address)), 0.0, 0.0, 0.0);
        if (source == OperandSource_ConstantVector3)
            return float4(asfloat(Constants.Load3(address)), 0.0);
        if (source == OperandSource_ConstantVector4)
            return asfloat(Constants.Load4(address));
        if (source == OperandSource_Parameter)
            return Parameters[Instance.ParameterBaseOffset + index].Value;
        if (source == OperandSource_DynamicInput)
            return float4(ResolveDynamicInput(index, InputContext, ParticleSeed), 0.0, 0.0, 0.0);
        if (source == OperandSource_ParticleAttribute)
            return GetAttribute(Attributes, index);
        if (source == OperandSource_ImmediateUInt)
            return float4(float(index), 0.0, 0.0, 0.0);

        return 0.0;
    }

    // Reads a scalar component from the captured attributes.
    float ReadScalarAttribute(uint index)
    {
        switch (index)
        {
            case 2u:  return Attributes.Rotation.x;
            case 3u:  return Attributes.Scale.x;
            case 6u:  return Attributes.Size.x;
            case 7u:  return Attributes.Lifetime.x;
            case 9u:  return Attributes.RibbonId.x;
            case 10u: return Attributes.Temp0.x;
            case 11u: return Attributes.Temp1.x;
            case 12u: return Attributes.Temp2.x;
            case 13u: return Attributes.Temp3.x;
            default:  return 0.0;
        }
    }

    // Reads three components from the captured attributes.
    float3 ReadVector3Attribute(uint index)
    {
        switch (index)
        {
            case 1u:  return Attributes.Position.xyz;
            case 4u:  return Attributes.Velocity.xyz;
            case 8u:  return Attributes.Tangent.xyz;
            case 10u: return Attributes.Temp0.xyz;
            case 11u: return Attributes.Temp1.xyz;
            case 12u: return Attributes.Temp2.xyz;
            case 13u: return Attributes.Temp3.xyz;
            default:  return 0.0;
        }
    }

    // Decodes a validated scalar without consulting the cache.
    float DecodeScalar(uint argument)
    {
        uint reference = GetOperandReference(argument);

        uint index = reference & OperandIndexMask;

        switch (reference >> 29)
        {
            case OperandSource_Constant:
                return asfloat(Constants.Load((Instance.ConstantBaseOffset + index) * 4u));
            case OperandSource_Parameter:
                return Parameters[Instance.ParameterBaseOffset + index].Value.x;
            case OperandSource_DynamicInput:
                return ResolveDynamicInput(index, InputContext, ParticleSeed);
            case OperandSource_ParticleAttribute:
                return ReadScalarAttribute(index);
            default:
                return 0.0;
        }
    }

    // Decodes a validated vector without consulting the cache.
    float3 DecodeVector3(uint argument)
    {
        uint reference = GetOperandReference(argument);
        uint index = reference & OperandIndexMask;

        switch (reference >> 29)
        {
            case OperandSource_ConstantVector3:
                return asfloat(Constants.Load3((Instance.ConstantBaseOffset + index) * 4u));
            case OperandSource_Parameter:
                return Parameters[Instance.ParameterBaseOffset + index].Value.xyz;
            case OperandSource_ParticleAttribute:
                return ReadVector3Attribute(index);
            default:
                return 0.0;
        }
    }

    /** @brief Returns a numeric value with the operation's required component count. */
    float4 ReadValue(uint argument)
    {
        if (argument < 2u) return CachedValues[argument];
        return DecodeValue(argument);
    }

    /** @brief Returns an operand validated by the compiler as a scalar. */
    float ReadScalar(uint argument)
    {
        if (argument < 2u) return CachedValues[argument].x;
        return DecodeScalar(argument);
    }

    /** @brief Returns an operand validated by the compiler as a three-component vector. */
    float3 ReadVector3(uint argument)
    {
        if (argument < 2u) return CachedValues[argument].xyz;
        return DecodeVector3(argument);
    }

    /** @brief Returns an unsigned integer without converting its payload to float. */
    uint ReadUInt(uint argument)
    {
        uint reference = GetOperandReference(argument);
        uint index = reference & OperandIndexMask;

        if ((reference >> 29) == OperandSource_ImmediateUInt)
            return index;

        return Constants.Load((Instance.ConstantBaseOffset + index) * 4u);
    }

    /** @brief Returns the curve parameter index, or -1 when the curve is missing. */
    int ReadCurve(uint argument)
    {
        uint index = GetOperandReference(argument) & OperandIndexMask;
        if (index != OperandIndexMask) return int(Instance.ParameterBaseOffset + index);
        return -1;
    }
};

/** @brief Captures the operand context and predecodes the first two numeric positions. */
SOperandReader CreateOperandReader(
    SOperation operation,
    StructuredBuffer<uint> operands,
    ByteAddressBuffer constants,
    StructuredBuffer<Parameter> parameters,
    SystemInstance instance,
    AttributeTable attributes,
    float inputContext,
    float particleSeed
)
{
    SOperandReader reader;
    reader.Operation = operation;
    reader.Operands = operands;
    reader.Constants = constants;
    reader.Parameters = parameters;
    reader.Instance = instance;
    reader.Attributes = attributes;
    reader.InputContext = inputContext;
    reader.ParticleSeed = particleSeed;
    reader.CachedValues[1] = 0.0;

    reader.CachedValues[0] = reader.DecodeValue(0u);

    if (operation.OperandCount > 1u)
        reader.CachedValues[1] = reader.DecodeValue(1u);

    return reader;
}

#endif
