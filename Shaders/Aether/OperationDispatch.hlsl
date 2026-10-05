#ifndef AETHER_OPERATION_DISPATCH_HLSL
#define AETHER_OPERATION_DISPATCH_HLSL

#include "Operations/SetLiteral.hlsl"
#include "Operations/RandomRange.hlsl"
#include "Operations/SampleDisk.hlsl"
#include "Operations/SampleCone.hlsl"
#include "Operations/SampleBox.hlsl"
#include "Operations/SetPositionOnCircle.hlsl"
#include "Operations/SetPositionCircularPath.hlsl"
#include "Operations/SetPositionVortexRibbonPath.hlsl"
#include "Operations/SetRibbonIdFromSpawnOrder.hlsl"
#include "Operations/AddWithDelta.hlsl"
#include "Operations/Dampen.hlsl"
#include "Operations/LerpOverLife.hlsl"
#include "Operations/KillOutsideBounds.hlsl"
#include "Operations/AddFromAttribute.hlsl"
#include "Operations/SampleCurve.hlsl"
#include "Operations/SampleColorCurve.hlsl"
#include "Operations/Add.hlsl"
#include "Operations/Mul.hlsl"
#include "Operations/Clamp.hlsl"
#include "Operations/CopyFromAttribute.hlsl"
#include "Operations/ApplyVortex.hlsl"

void ExecuteOperation(
    inout SOperationExecutor executor,
    inout SOperationExecutorContext context
)
{
    if (executor.Reader.Operation.Type == Operation_SetLiteral)
        SetLiteral(executor, context);
    else if (executor.Reader.Operation.Type == Operation_RandomRange)
        RandomRange(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SampleDisk)
        SampleDisk(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SampleCone)
        SampleCone(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SampleBox)
        SampleBox(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SetPositionOnCircle)
        SetPositionOnCircle(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SetPositionCircularPath)
        SetPositionCircularPath(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SetPositionVortexRibbonPath)
        SetPositionVortexRibbonPath(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SetRibbonIdFromSpawnOrder)
        SetRibbonIdFromSpawnOrder(executor, context);
    else if (executor.Reader.Operation.Type == Operation_AddWithDelta)
        AddWithDelta(executor, context);
    else if (executor.Reader.Operation.Type == Operation_Dampen)
        Dampen(executor, context);
    else if (executor.Reader.Operation.Type == Operation_LerpOverLife)
        LerpOverLife(executor, context);
    else if (executor.Reader.Operation.Type == Operation_KillOutsideBounds)
        KillOutsideBounds(executor, context);
    else if (executor.Reader.Operation.Type == Operation_AddFromAttribute)
        AddFromAttribute(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SampleCurve)
        SampleCurve(executor, context);
    else if (executor.Reader.Operation.Type == Operation_SampleColorCurve)
        SampleColorCurve(executor, context);
    else if (executor.Reader.Operation.Type == Operation_Add)
        Add(executor, context);
    else if (executor.Reader.Operation.Type == Operation_Mul)
        Mul(executor, context);
    else if (executor.Reader.Operation.Type == Operation_Clamp)
        Clamp(executor, context);
    else if (executor.Reader.Operation.Type == Operation_CopyFromAttribute)
        CopyFromAttribute(executor, context);
    else if (executor.Reader.Operation.Type == Operation_ApplyVortex)
        ApplyVortex(executor, context);
}

#endif
