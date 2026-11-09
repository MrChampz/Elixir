#ifndef AETHER_OPERATION_SETGROUPIDFROMSPAWNORDER_HLSL
#define AETHER_OPERATION_SETGROUPIDFROMSPAWNORDER_HLSL

static const uint Operation_SetGroupIdFromSpawnOrder = 13u;
static const uint SetGroupIdFromSpawnOrder_GroupCount = 0u;
static const uint SetGroupIdFromSpawnOrder_FirstGroupId = 1u;

void SetGroupIdFromSpawnOrder(
    inout SOperationExecutor executor,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    uint groupCount = max(1u, resolver.ReadUInt(SetGroupIdFromSpawnOrder_GroupCount));
    uint firstGroupId = resolver.ReadUInt(SetGroupIdFromSpawnOrder_FirstGroupId);
    uint groupId = firstGroupId + (context.EmissionIndex % groupCount);

    SetAttribute(
        context.Attributes,
        resolver.Operation.Target,
        groupId
    );
}

#endif
