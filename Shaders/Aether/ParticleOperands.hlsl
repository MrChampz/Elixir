#define AETHER_OPERAND_SOURCE(Name, Tag) static const uint Source_##Name = Tag;
#include "../../Elixir/Source/Engine/Aether/Core/ParticleOperandSources.def"
#undef AETHER_OPERAND_SOURCE

// Argument positions and opcodes come from the CPU/GPU contract.
#define AETHER_OPERATION(Name, Opcode, ...) static const uint Op_##Name = Opcode;
#define AETHER_ARGUMENT(Op, Name, Index) static const uint Op##_##Name = Index;
#include "../../Elixir/Source/Engine/Aether/Core/ParticleOperations.def"
#undef AETHER_ARGUMENT
#undef AETHER_OPERATION

[[vk::binding(7, 0)]]
StructuredBuffer<uint> operands;
[[vk::binding(8, 0)]]
ByteAddressBuffer constantWords;

uint OperandReference(Op op, uint argument, SystemInstance instance)
{
    return operands[instance.OperandBaseOffset + op.OperandOffset + argument];
}

float4 ReadOperand(Op op, uint argument, SystemInstance instance, AttributeTable attributes, float inputContext, float particleSeed)
{
    uint reference = OperandReference(op, argument, instance);
    uint source = reference >> 29;
    uint index = reference & 0x1fffffffu;
    uint address = (instance.ConstantBaseOffset + index) * 4u;
    if (source == Source_Constant) return float4(asfloat(constantWords.Load(address)), 0.0, 0.0, 0.0);
    if (source == Source_ConstantVector3) return float4(asfloat(constantWords.Load3(address)), 0.0);
    if (source == Source_ConstantVector4) return asfloat(constantWords.Load4(address));
    if (source == Source_Parameter) return parameters[instance.ParameterBaseOffset + index].Value;
    if (source == Source_DynamicInput) return float4(ResolveDynamicInput(index, inputContext, particleSeed), 0.0, 0.0, 0.0);
    if (source == Source_ParticleAttribute) return GetAttribute(attributes, index);
    if (source == Source_ImmediateUInt) return float4(float(index), 0.0, 0.0, 0.0);
    return 0.0;
}

uint ReadUIntOperand(Op op, uint argument, SystemInstance instance)
{
    uint reference = OperandReference(op, argument, instance);
    uint index = reference & 0x1fffffffu;
    return (reference >> 29) == Source_ImmediateUInt ? index : constantWords.Load((instance.ConstantBaseOffset + index) * 4u);
}

int ReadCurveOperand(Op op, uint argument, SystemInstance instance)
{
    uint index = OperandReference(op, argument, instance) & 0x1fffffffu;
    return index == 0x1fffffffu ? -1 : int(instance.ParameterBaseOffset + index);
}
