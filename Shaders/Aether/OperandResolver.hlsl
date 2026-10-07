#ifndef AETHER_OPERAND_READER_HLSL
#define AETHER_OPERAND_READER_HLSL

#include "Operand.hlsl"

/** @brief Reads operation operands from a snapshot taken before destination writes. */
struct SOperandResolver
{
    StructuredBuffer<uint> Operands;
    ByteAddressBuffer Constants;
    StructuredBuffer<SGPUParameter> Parameters;

    SGPUOperation Operation;
    SGPUSystemInstance Instance;
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
            case ParticleAttribute_Rotation:  return Attributes.Rotation.x;
            case ParticleAttribute_Scale:  return Attributes.Scale.x;
            case ParticleAttribute_Size:  return Attributes.Size.x;
            case ParticleAttribute_Lifetime:  return Attributes.Lifetime.x;
            case ParticleAttribute_Temp0: return Attributes.Temp0.x;
            case ParticleAttribute_Temp1: return Attributes.Temp1.x;
            case ParticleAttribute_Temp2: return Attributes.Temp2.x;
            case ParticleAttribute_Temp3: return Attributes.Temp3.x;
            default:  return 0.0;
        }
    }

    // Reads three components from the captured attributes.
    float3 ReadVector3Attribute(uint index)
    {
        switch (index)
        {
            case ParticleAttribute_Position:  return Attributes.Position.xyz;
            case ParticleAttribute_Velocity:  return Attributes.Velocity.xyz;
            case ParticleAttribute_Tangent:  return Attributes.Tangent.xyz;
            case ParticleAttribute_Temp0: return Attributes.Temp0.xyz;
            case ParticleAttribute_Temp1: return Attributes.Temp1.xyz;
            case ParticleAttribute_Temp2: return Attributes.Temp2.xyz;
            case ParticleAttribute_Temp3: return Attributes.Temp3.xyz;
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
        if ((reference >> 29) == OperandSource_ParticleAttribute && index == ParticleAttribute_RibbonId)
            return Attributes.RibbonId;

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
SOperandResolver CreateOperandResolver(
    SGPUOperation operation,
    StructuredBuffer<uint> operands,
    ByteAddressBuffer constants,
    StructuredBuffer<SGPUParameter> parameters,
    SGPUSystemInstance instance,
    AttributeTable attributes,
    float inputContext,
    float particleSeed
)
{
    SOperandResolver resolver;
    resolver.Operation = operation;
    resolver.Operands = operands;
    resolver.Constants = constants;
    resolver.Parameters = parameters;
    resolver.Instance = instance;
    resolver.Attributes = attributes;
    resolver.InputContext = inputContext;
    resolver.ParticleSeed = particleSeed;
    resolver.CachedValues[1] = 0.0;

    resolver.CachedValues[0] = resolver.DecodeValue(0u);

    if (operation.OperandCount > 1u)
        resolver.CachedValues[1] = resolver.DecodeValue(1u);

    return resolver;
}

#endif
