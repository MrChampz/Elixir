#include "../../../Shaders/Aether/ParticleData.hlsl"
#include "../../../Shaders/Aether/ParticleAttributes.hlsl"
#include "../../../Shaders/Aether/Operation.hlsl"
#include "../../../Shaders/Aether/OperandResolver.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<uint> operands;
[[vk::binding(1, 0)]] ByteAddressBuffer constants;
[[vk::binding(2, 0)]] ByteAddressBuffer parameters;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> results;

[numthreads(1, 1, 1)]
void main()
{
    SGPUOperation operation = (SGPUOperation)0;
    operation.OperandCount = 4;
    SOperandResolver resolver = CreateOperandResolver(
        operation,
        operands,
        constants,
        parameters,
        (SGPUSystemInstance)0,
        (AttributeTable)0,
        (SDynamicInputContext)0
    );
    results[0] = resolver.ReadUInt(0);
    results[1] = resolver.ReadUInt(1);
    results[2] = resolver.ReadUInt(2);
    results[3] = resolver.ReadUInt(3);
}
