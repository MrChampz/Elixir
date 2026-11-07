#include "../../../Shaders/Aether/ParticleData.hlsl"
#include "../../../Shaders/Aether/ParticleAttributes.hlsl"
#include "../../../Shaders/Aether/Operation.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<SGPUOperation> operations;
[[vk::binding(1, 0)]] StructuredBuffer<uint> operands;
[[vk::binding(2, 0)]] ByteAddressBuffer constants;
[[vk::binding(3, 0)]] ByteAddressBuffer parameters;
[[vk::binding(4, 0)]] RWStructuredBuffer<float4> results;

#include "../../../Shaders/Aether/OperationExecutor.hlsl"

[numthreads(1, 1, 1)]
void main()
{
    SOperationExecutor executor = CreateOperationExecutor(operands, constants, parameters);
    SOperationExecutorContext context = (SOperationExecutorContext)0;
    context.Attributes.Position = float4(0.2, 0.3, 0.4, 0.0);
    context.Attributes.Velocity = float4(1.0, 2.0, 3.0, 0.0);
    context.DeltaTime = 0.1;
    context.Time = 2.1;
    context.DynamicInputs.Time = context.Time;
    context.DynamicInputs.ParticleSeed = 0.37;
    uint count;
    uint stride;
    operations.GetDimensions(count, stride);
    for (uint index = 0; index < count; ++index)
        executor.Execute(operations[index], context);
    results[0] = context.Attributes.Velocity;
    results[2] = context.Attributes.Position;

    // Reference the original force equation independently of the compiled instruction sequence.
    float strength = max(asfloat(parameters.Load(0)), 0.0);
    float frequency = max(asfloat(parameters.Load(4)), 0.000001);
    float3 scroll = asfloat(parameters.Load3(8));
    uint seedBits = parameters.Load(20);
    float seed = 0.37 + float(seedBits & 65535u) + float(seedBits >> 16u) * 0.017;
    float3 coordinates = float3(0.2, 0.3, 0.4) * frequency +
        float3(seed * 37, seed * 21, seed * 11) + 2.1 * scroll;
    results[1] = float4(float3(1.0, 2.0, 3.0) + CurlNoise3(coordinates) * strength * 0.1, 0.0);
}
