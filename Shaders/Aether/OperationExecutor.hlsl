#ifndef AETHER_OPERATION_EXECUTOR_HLSL
#define AETHER_OPERATION_EXECUTOR_HLSL

#include "OperandResolver.hlsl"

/** @brief Stores particle inputs and mutable state for a sequence of operations. */
struct SOperationExecutorContext
{
    SGPUSystemInstance Instance;
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

struct SOperationExecutor;

// Dispatches through the operation set selected by the shader entry point.
void ExecuteOperation(
    inout SOperationExecutor executor,
    SGPUOperation operation,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
);

/** @brief Executes operations with shared context and a fresh operand snapshot. */
struct SOperationExecutor
{
    StructuredBuffer<uint> Operands;
    ByteAddressBuffer Constants;
    ByteAddressBuffer Parameters;

    /** @brief Applies the operation to the shared attributes and discard state. */
    void Execute(SGPUOperation operation, inout SOperationExecutorContext context)
    {
        SOperandResolver resolver = CreateOperandResolver(
            operation,
            Operands,
            Constants,
            Parameters,
            context.Instance,
            context.Attributes,
            context.InputContext,
            context.ParticleSeed
        );
        ExecuteOperation(this, operation, resolver, context);
    }
};

SOperationExecutor CreateOperationExecutor(
    StructuredBuffer<uint> operands,
    ByteAddressBuffer constants,
    ByteAddressBuffer parameters
)
{
    SOperationExecutor executor;
    executor.Operands = operands;
    executor.Constants = constants;
    executor.Parameters = parameters;

    return executor;
}

#include "OperationDispatch.hlsl"

#endif
