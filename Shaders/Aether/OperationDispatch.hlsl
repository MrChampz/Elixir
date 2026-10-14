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
#include "Operations/ApplyCurlNoise.hlsl"

void ExecuteOperation(
    inout SOperationExecutor executor,
    SGPUOperation operation,
    SOperandResolver resolver,
    inout SOperationExecutorContext context
)
{
    if (operation.Type == Operation_ApplyCurlNoise)
        ApplyCurlNoise(executor, resolver, context);
    else if (operation.Type == Operation_SetLiteral)
        SetLiteral(executor, resolver, context);
    else if (operation.Type == Operation_RandomRange)
        RandomRange(executor, resolver, context);
    else if (operation.Type == Operation_SampleDisk)
        SampleDisk(executor, resolver, context);
    else if (operation.Type == Operation_SampleCone)
        SampleCone(executor, resolver, context);
    else if (operation.Type == Operation_SampleBox)
        SampleBox(executor, resolver, context);
    else if (operation.Type == Operation_SetPositionOnCircle)
        SetPositionOnCircle(executor, resolver, context);
    else if (operation.Type == Operation_SetPositionCircularPath)
        SetPositionCircularPath(executor, resolver, context);
    else if (operation.Type == Operation_SetPositionVortexRibbonPath)
        SetPositionVortexRibbonPath(executor, resolver, context);
    else if (operation.Type == Operation_SetRibbonIdFromSpawnOrder)
        SetRibbonIdFromSpawnOrder(executor, resolver, context);
    else if (operation.Type == Operation_AddWithDelta)
        AddWithDelta(executor, resolver, context);
    else if (operation.Type == Operation_Dampen)
        Dampen(executor, resolver, context);
    else if (operation.Type == Operation_LerpOverLife)
        LerpOverLife(executor, resolver, context);
    else if (operation.Type == Operation_KillOutsideBounds)
        KillOutsideBounds(executor, resolver, context);
    else if (operation.Type == Operation_AddFromAttribute)
        AddFromAttribute(executor, resolver, context);
    else if (operation.Type == Operation_SampleCurve)
        SampleCurve(executor, resolver, context);
    else if (operation.Type == Operation_SampleColorCurve)
        SampleColorCurve(executor, resolver, context);
    else if (operation.Type == Operation_Add)
        Add(executor, resolver, context);
    else if (operation.Type == Operation_Mul)
        Mul(executor, resolver, context);
    else if (operation.Type == Operation_Clamp)
        Clamp(executor, resolver, context);
    else if (operation.Type == Operation_CopyFromAttribute)
        CopyFromAttribute(executor, resolver, context);
    else if (operation.Type == Operation_ApplyVortex)
        ApplyVortex(executor, resolver, context);
}

#endif
