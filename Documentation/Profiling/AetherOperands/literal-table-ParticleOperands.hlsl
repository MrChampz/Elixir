static const uint Source_Constant           = 0;
static const uint Source_Parameter          = 1;
static const uint Source_DynamicInput       = 2;
static const uint Source_ParticleAttribute  = 3;
static const uint Source_ImmediateUInt      = 4;
static const uint Source_Curve              = 5;
static const uint Source_ConstantVector3    = 6;
static const uint Source_ConstantVector4    = 7;

#include "ParticleOperations.hlsl"

[[vk::binding(7, 0)]]
StructuredBuffer<uint> operands;
[[vk::binding(8, 0)]]
ByteAddressBuffer constants;

uint OperandReference(Op op, uint argument, SystemInstance instance)
{
    return operands[instance.OperandBaseOffset + op.OperandOffset + argument];
}

float4 ReadOperand(
    Op op,
    uint argument,
    SystemInstance instance,
    AttributeTable attributes,
    float inputContext,
    float particleSeed
)
{
    uint reference = OperandReference(op, argument, instance);

    uint source = reference >> 29;
    uint index = reference & 0x1fffffffu;
    uint address = (instance.ConstantBaseOffset + index) * 4u;

    if (source == Source_Constant)
        return float4(asfloat(constants.Load(address)), 0.0, 0.0, 0.0);
    if (source == Source_ConstantVector3)
        return float4(asfloat(constants.Load3(address)), 0.0);
    if (source == Source_ConstantVector4)
        return asfloat(constants.Load4(address));
    if (source == Source_Parameter)
        return parameters[instance.ParameterBaseOffset + index].Value;
    if (source == Source_DynamicInput)
        return float4(ResolveDynamicInput(index, inputContext, particleSeed), 0.0, 0.0, 0.0);
    if (source == Source_ParticleAttribute)
        return GetAttribute(attributes, index);
    if (source == Source_ImmediateUInt)
        return float4(float(index), 0.0, 0.0, 0.0);

    return 0.0;
}

float ReadScalarAttribute(AttributeTable attributes, uint index)
{
    switch (index)
    {
        case 2u:  return attributes.Rotation.x;
        case 3u:  return attributes.Scale.x;
        case 6u:  return attributes.Size.x;
        case 7u:  return attributes.Lifetime.x;
        case 9u:  return attributes.RibbonId.x;
        case 10u: return attributes.Temp0.x;
        case 11u: return attributes.Temp1.x;
        case 12u: return attributes.Temp2.x;
        case 13u: return attributes.Temp3.x;
        default:  return 0.0;
    }
}

float3 ReadVector3Attribute(AttributeTable attributes, uint index)
{
    switch (index)
    {
        case 1u:  return attributes.Position.xyz;
        case 4u:  return attributes.Velocity.xyz;
        case 8u:  return attributes.Tangent.xyz;
        case 10u: return attributes.Temp0.xyz;
        case 11u: return attributes.Temp1.xyz;
        case 12u: return attributes.Temp2.xyz;
        case 13u: return attributes.Temp3.xyz;
        default:  return 0.0;
    }
}

// Scalar contracts only admit scalar constants, parameters, inputs, and attributes.
float ReadScalarOperand(
    Op op,
    uint argument,
    SystemInstance instance,
    AttributeTable attributes,
    float inputContext,
    float particleSeed
)
{
    uint reference = OperandReference(op, argument, instance);

    uint index = reference & 0x1fffffffu;

    switch (reference >> 29)
    {
        case Source_Constant:
            return asfloat(constants.Load((instance.ConstantBaseOffset + index) * 4u));
        case Source_Parameter:
            return parameters[instance.ParameterBaseOffset + index].Value.x;
        case Source_DynamicInput:
            return ResolveDynamicInput(index, inputContext, particleSeed);
        case Source_ParticleAttribute:
            return ReadScalarAttribute(attributes, index);
        default:
            return 0.0;
    }
}

// Vector contracts do not need dynamic-input or scalar decoding.
float3 ReadVector3Operand(
    Op op,
    uint argument,
    SystemInstance instance,
    AttributeTable attributes
)
{
    uint reference = OperandReference(op, argument, instance);
    uint index = reference & 0x1fffffffu;

    switch (reference >> 29)
    {
        case Source_ConstantVector3:
            return asfloat(constants.Load3((instance.ConstantBaseOffset + index) * 4u));
        case Source_Parameter:
            return parameters[instance.ParameterBaseOffset + index].Value.xyz;
        case Source_ParticleAttribute:
            return ReadVector3Attribute(attributes, index);
        default:
            return 0.0;
    }
}

uint ReadUIntOperand(Op op, uint argument, SystemInstance instance)
{
    uint reference = OperandReference(op, argument, instance);
    uint index = reference & 0x1fffffffu;

    if ((reference >> 29) == Source_ImmediateUInt)
        return index;

    return constants.Load((instance.ConstantBaseOffset + index) * 4u);
}

int ReadCurveOperand(Op op, uint argument, SystemInstance instance)
{
    uint index = OperandReference(op, argument, instance) & 0x1fffffffu;
    if (index != 0x1fffffffu) return int(instance.ParameterBaseOffset + index);
    return -1;
}

/** @brief Reads operation operands from a snapshot taken before destination writes. */
struct OperandReader
{
    Op Operation;
    SystemInstance Instance;
    AttributeTable Attributes;
    float InputContext;
    float ParticleSeed;

    // Predecode the common numeric positions before selecting the operation.
    float4 CachedValues[2];

    /** @brief Returns a numeric value with the operation's required component count. */
    float4 ReadValue(uint argument)
    {
        if (argument < 2u) return CachedValues[argument];
        return ReadOperand(
            Operation,
            argument,
            Instance,
            Attributes,
            InputContext,
            ParticleSeed
        );
    }

    /** @brief Returns an operand validated by the compiler as a scalar. */
    float ReadScalar(uint argument)
    {
        if (argument < 2u) return CachedValues[argument].x;
        return ReadScalarOperand(
            Operation,
            argument,
            Instance,
            Attributes,
            InputContext,
            ParticleSeed
        );
    }

    /** @brief Returns an operand validated by the compiler as a three-component vector. */
    float3 ReadVector3(uint argument)
    {
        if (argument < 2u) return CachedValues[argument].xyz;
        return ReadVector3Operand(Operation, argument, Instance, Attributes);
    }

    /** @brief Returns an unsigned integer without converting its payload to float. */
    uint ReadUInt(uint argument)
    {
        return ReadUIntOperand(Operation, argument, Instance);
    }

    /** @brief Returns the curve parameter index, or -1 when the curve is missing. */
    int ReadCurve(uint argument)
    {
        return ReadCurveOperand(Operation, argument, Instance);
    }
};

/** @brief Captures the operand context and predecodes the first two numeric positions. */
OperandReader CreateOperandReader(
    Op op,
    SystemInstance instance,
    AttributeTable attributes,
    float inputContext,
    float particleSeed
)
{
    OperandReader reader = (OperandReader)0;
    reader.Operation = op;
    reader.Instance = instance;
    reader.Attributes = attributes;
    reader.InputContext = inputContext;
    reader.ParticleSeed = particleSeed;

    reader.CachedValues[0] = ReadOperand(
        op,
        0u,
        instance,
        attributes,
        inputContext,
        particleSeed
    );

    if (op.GetOperandCount() > 1u)
        reader.CachedValues[1] = ReadOperand(
            op,
            1u,
            instance,
            attributes,
            inputContext,
            particleSeed
        );

    return reader;
}
