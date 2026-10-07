#ifndef AETHER_OPERATION_EXECUTOR_HLSL
#define AETHER_OPERATION_EXECUTOR_HLSL

#include "OperandReader.hlsl"

/** @brief Stores shared resources and mutable state for a sequence of operations. */
struct SOperationExecutorContext
{
    StructuredBuffer<uint> Operands;
    ByteAddressBuffer Constants;
    StructuredBuffer<Parameter> Parameters;
    SystemInstance Instance;
    float InputContext;
    float ParticleSeed;
    AttributeTable Attributes;
    float DeltaTime;
    float NormalizedAge;
    float Time;
    float2 SeedBase;
    uint OperationIndex;
    float SpawnRate;
    uint SpawnCount;
    uint SpawnOrder;
    uint EmissionIndex;
    bool Kill;
};

/** @brief Initializes the context before executing a particle's operation sequence. */
SOperationExecutorContext CreateOperationExecutorContext(
    StructuredBuffer<uint> operands,
    ByteAddressBuffer constants,
    StructuredBuffer<Parameter> parameters,
    SystemInstance instance,
    AttributeTable attributes,
    float inputContext,
    float particleSeed
)
{
    SOperationExecutorContext context;
    context.Operands = operands;
    context.Constants = constants;
    context.Parameters = parameters;
    context.Instance = instance;
    context.InputContext = inputContext;
    context.ParticleSeed = particleSeed;
    context.Attributes = attributes;
    context.DeltaTime = 0.0;
    context.NormalizedAge = 0.0;
    context.Time = 0.0;
    context.SeedBase = 0.0;
    context.OperationIndex = 0u;
    context.SpawnRate = 0.0;
    context.SpawnCount = 0u;
    context.SpawnOrder = 0u;
    context.EmissionIndex = 0u;
    context.Kill = false;
    return context;
}

struct SOperationExecutor;

// Dispatches through the operation set selected by the shader entry point.
void ExecuteOperation(
    inout SOperationExecutor executor,
    inout SOperationExecutorContext context
);

/** @brief Executes one operation using a snapshot of its operand values. */
struct SOperationExecutor
{
    SOperandReader Reader;

    /** @brief Applies the operation to the shared attributes and discard state. */
    void Execute(inout SOperationExecutorContext context)
    {
        ExecuteOperation(this, context);
    }
};

#include "OperationDispatch.hlsl"

/** @brief Captures operand values from the context before destination writes. */
SOperationExecutor CreateOperationExecutor(
    SGPUOperation operation,
    SOperationExecutorContext context
)
{
    SOperationExecutor executor;
    executor.Reader = CreateOperandReader(
        operation,
        context.Operands,
        context.Constants,
        context.Parameters,
        context.Instance,
        context.Attributes,
        context.InputContext,
        context.ParticleSeed
    );

    return executor;
}

#endif
