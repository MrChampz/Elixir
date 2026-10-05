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
ByteAddressBuffer constantWords;

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

    if (argument < 16u && ((op.OperandMetadata >> 16) & (1u << argument)) != 0u)
        return float4(asfloat(reference), 0.0, 0.0, 0.0);

    uint source = reference >> 29;
    uint index = reference & 0x1fffffffu;
    uint address = (instance.ConstantBaseOffset + index) * 4u;

    if (source == Source_Constant)
        return float4(asfloat(constantWords.Load(address)), 0.0, 0.0, 0.0);
    if (source == Source_ConstantVector3)
        return float4(asfloat(constantWords.Load3(address)), 0.0);
    if (source == Source_ConstantVector4)
        return asfloat(constantWords.Load4(address));
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

// Constant argument indices let DXC eliminate the cache selection and fallback.
float4 ReadCachedOperand(Op op, uint argument, SystemInstance instance, AttributeTable attributes, float inputContext, float particleSeed, float4 arguments[2])
{
    if (argument < 2u) return arguments[argument];
    return ReadOperand(op, argument, instance, attributes, inputContext, particleSeed);
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

    if (argument < 16u && ((op.OperandMetadata >> 16) & (1u << argument)) != 0u)
        return asfloat(reference);

    uint index = reference & 0x1fffffffu;

    switch (reference >> 29)
    {
        case Source_Constant:
            return asfloat(constantWords.Load((instance.ConstantBaseOffset + index) * 4u));
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
            return asfloat(constantWords.Load3((instance.ConstantBaseOffset + index) * 4u));
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

    return constantWords.Load((instance.ConstantBaseOffset + index) * 4u);
}

int ReadCurveOperand(Op op, uint argument, SystemInstance instance)
{
    uint index = OperandReference(op, argument, instance) & 0x1fffffffu;
    if (index != 0x1fffffffu) return int(instance.ParameterBaseOffset + index);
    return -1;
}
