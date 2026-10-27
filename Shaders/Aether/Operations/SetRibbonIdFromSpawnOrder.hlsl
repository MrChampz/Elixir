#ifndef AETHER_OPERATION_SETRIBBONIDFROMSPAWNORDER_HLSL
#define AETHER_OPERATION_SETRIBBONIDFROMSPAWNORDER_HLSL

static const uint Operation_SetRibbonIdFromSpawnOrder = 13u;
static const uint SetRibbonIdFromSpawnOrder_RibbonCount = 0u;
static const uint SetRibbonIdFromSpawnOrder_FirstRibbonId = 1u;

void SetRibbonIdFromSpawnOrder(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    uint ribbonCount = max(1u, resolver.ReadUInt(SetRibbonIdFromSpawnOrder_RibbonCount));
    uint firstRibbonId = resolver.ReadUInt(SetRibbonIdFromSpawnOrder_FirstRibbonId);
    uint ribbonId = firstRibbonId + (context.EmissionIndex % ribbonCount);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        ribbonId
    );
}

#endif
