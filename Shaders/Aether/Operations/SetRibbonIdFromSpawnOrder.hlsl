#ifndef AETHER_OPERATION_SETRIBBONIDFROMSPAWNORDER_HLSL
#define AETHER_OPERATION_SETRIBBONIDFROMSPAWNORDER_HLSL

static const uint Operation_SetRibbonIdFromSpawnOrder = 13u;
static const uint SetRibbonIdFromSpawnOrder_RibbonCount = 0u;
static const uint SetRibbonIdFromSpawnOrder_FirstRibbonId = 1u;

void SetRibbonIdFromSpawnOrder(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    uint ribbonCount = max(1u, executor.Reader.ReadUInt(SetRibbonIdFromSpawnOrder_RibbonCount));
    uint firstRibbonId = executor.Reader.ReadUInt(SetRibbonIdFromSpawnOrder_FirstRibbonId);
    uint ribbonId = firstRibbonId + (context.EmissionIndex % ribbonCount);
    float4 ribbonValue = float4(float(ribbonId), 0.0, 0.0, 0.0);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        ribbonValue
    );

}

#endif
