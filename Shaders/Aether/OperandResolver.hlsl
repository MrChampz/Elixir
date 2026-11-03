#ifndef AETHER_OPERAND_RESOLVER_HLSL
#define AETHER_OPERAND_RESOLVER_HLSL

#include "Operand.hlsl"
#include "DynamicInput.hlsl"

/** @brief Reads operation operands from a snapshot taken before destination writes. */
struct SOperandResolver
{
    StructuredBuffer<uint> Operands;
    ByteAddressBuffer Constants;
    ByteAddressBuffer Parameters;

    SGPUOperation Operation;
    SGPUSystemInstance Instance;
    AttributeTable Attributes;
    SDynamicInputContext DynamicInputs;

    // Predecode the common numeric positions before selecting the operation.
    float4 CachedValues[2];

    // Returns the packed source reference for an argument.
    uint GetOperandReference(uint argument)
    {
        return Operands[Instance.OperandBaseOffset + Operation.OperandOffset + argument];
    }

    // Returns the byte address within the operand's constant or parameter buffer.
    uint GetValueAddress(uint reference)
    {
        uint baseOffset = GetOperandSource(reference) == OperandSource_Parameter
            ? Instance.ParameterBaseOffset
            : Instance.ConstantBaseOffset;
        return (baseOffset + (reference & OperandIndexMask)) * 4u;
    }

    // Reads one raw word from the selected buffer.
    uint Load(uint reference, uint address)
    {
        if (GetOperandSource(reference) == OperandSource_Parameter)
            return Parameters.Load(address);
        return Constants.Load(address);
    }

    // Reads two raw words from the selected buffer.
    uint2 Load2(uint reference, uint address)
    {
        if (GetOperandSource(reference) == OperandSource_Parameter)
            return Parameters.Load2(address);
        return Constants.Load2(address);
    }

    // Reads three raw words from the selected buffer.
    uint3 Load3(uint reference, uint address)
    {
        if (GetOperandSource(reference) == OperandSource_Parameter)
            return Parameters.Load3(address);
        return Constants.Load3(address);
    }

    // Reads four raw words from the selected buffer.
    uint4 Load4(uint reference, uint address)
    {
        if (GetOperandSource(reference) == OperandSource_Parameter)
            return Parameters.Load4(address);
        return Constants.Load4(address);
    }

    // Each 16-bit half fits exactly in the existing float cache.
    float4 CacheUInt(uint value)
    {
        return float4(float(value & 0xffffu), float(value >> 16u), 0.0, 0.0);
    }

    // Decodes a numeric value without consulting the cache.
    float4 DecodeValue(uint argument)
    {
        uint reference = GetOperandReference(argument);

        uint source = GetOperandSource(reference);
        uint index = reference & OperandIndexMask;
        uint address = GetValueAddress(reference);

        if (source == OperandSource_Constant || source == OperandSource_Parameter)
        {
            uint format = GetOperandFormat(reference);

            if (format == OperandFormat_Float)
                return float4(asfloat(Load(reference, address)), 0.0, 0.0, 0.0);
            if (format == OperandFormat_Float3)
                return float4(asfloat(Load3(reference, address)), 0.0);
            if (format == OperandFormat_Float4)
                return asfloat(Load4(reference, address));
            if (format == OperandFormat_Float2)
                return float4(asfloat(Load2(reference, address)), 0.0, 0.0);
            if (format == OperandFormat_UInt)
                return CacheUInt(Load(reference, address));
        }

        if (source == OperandSource_DynamicInput)
            return float4(
                ResolveDynamicInput(index, DynamicInputs),
                0.0,
                0.0,
                0.0
            );

        if (source == OperandSource_ParticleAttribute)
            return index == ParticleAttribute_RibbonId
                ? CacheUInt(Attributes.RibbonId)
                : GetAttribute(Attributes, index);

        if (source == OperandSource_Immediate)
            return CacheUInt(index);

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

        switch (GetOperandSource(reference))
        {
            case OperandSource_Constant:
            case OperandSource_Parameter:
                return asfloat(Load(reference, GetValueAddress(reference)));
            case OperandSource_DynamicInput:
                return ResolveDynamicInput(index, DynamicInputs);
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

        uint source = GetOperandSource(reference);
        if (source == OperandSource_Constant || source == OperandSource_Parameter)
        {
            uint format = GetOperandFormat(reference);

            if (format == OperandFormat_Float2)
                return float3(asfloat(Load2(reference, GetValueAddress(reference))), 0.0);

            return asfloat(Load3(reference, GetValueAddress(reference)));
        }

        if (source == OperandSource_ParticleAttribute)
            return ReadVector3Attribute(index);

        return 0.0;
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

    /**
     * @brief Converts a numeric scalar to an unsigned integer, or zero for invalid values.
     * @param value Finite, nonnegative integer smaller than 2 to the power of 32.
     */
    uint ConvertScalarToUInt(float value)
    {
        if (!isfinite(value) || value < 0.0 || value >= 4294967296.0 || floor(value) != value)
            return 0u;
        return uint(value);
    }

    /** @brief Returns all 32 bits of an unsigned integer operand. */
    uint DecodeUInt(uint argument)
    {
        uint reference = GetOperandReference(argument);
        uint index = reference & OperandIndexMask;
        uint source = GetOperandSource(reference);

        if ((source == OperandSource_Constant || source == OperandSource_Parameter) &&
            GetOperandFormat(reference) == OperandFormat_UInt)
            return Load(reference, GetValueAddress(reference));

        if (source == OperandSource_Parameter &&
            GetOperandFormat(reference) == OperandFormat_Float)
            return ConvertScalarToUInt(asfloat(Load(reference, GetValueAddress(reference))));

        if (source == OperandSource_Immediate)
            return index;

        if (source == OperandSource_ParticleAttribute && index == ParticleAttribute_RibbonId)
            return Attributes.RibbonId;

        return 0u;
    }

    uint ReadUInt(uint argument)
    {
        if (argument < 2u)
        {
            float4 value = CachedValues[argument];
            uint reference = GetOperandReference(argument);
            if (GetOperandSource(reference) == OperandSource_Parameter &&
                GetOperandFormat(reference) == OperandFormat_Float)
                return ConvertScalarToUInt(value.x);
            return uint(value.x) | (uint(value.y) << 16u);
        }
        return DecodeUInt(argument);
    }

    /** @brief Returns the curve's absolute word offset, or -1 when the curve is missing. */
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
    ByteAddressBuffer parameters,
    SGPUSystemInstance instance,
    AttributeTable attributes,
    SDynamicInputContext dynamicInputs
)
{
    SOperandResolver resolver;
    resolver.Operation = operation;
    resolver.Operands = operands;
    resolver.Constants = constants;
    resolver.Parameters = parameters;
    resolver.Instance = instance;
    resolver.Attributes = attributes;
    resolver.DynamicInputs = dynamicInputs;
    resolver.CachedValues[0] = 0.0;
    resolver.CachedValues[1] = 0.0;
    resolver.CachedValues[0] = resolver.DecodeValue(0u);
    if (operation.OperandCount > 1u)
        resolver.CachedValues[1] = resolver.DecodeValue(1u);

    return resolver;
}

#endif
