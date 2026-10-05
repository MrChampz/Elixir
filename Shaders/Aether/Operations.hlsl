#ifndef AETHER_OPERATIONS_HLSL
#define AETHER_OPERATIONS_HLSL

struct SOperation
{
    uint Type;
    uint Target;
    uint OperandOffset;
    uint OperandCount;
};

static const uint Operation_SetLiteral = 0u;
static const uint SetLiteral_Value = 0u;

static const uint Operation_RandomRange = 1u;
static const uint RandomRange_Minimum = 0u;
static const uint RandomRange_Maximum = 1u;

static const uint Operation_SampleDisk = 2u;
static const uint SampleDisk_Center = 0u;
static const uint SampleDisk_Radius = 1u;
static const uint SampleDisk_Normal = 2u;

static const uint Operation_SampleCone = 3u;
static const uint SampleCone_Direction = 0u;
static const uint SampleCone_Angle = 1u;
static const uint SampleCone_MinSpeed = 2u;
static const uint SampleCone_MaxSpeed = 3u;

static const uint Operation_SampleBox = 4u;
static const uint SampleBox_Minimum = 0u;
static const uint SampleBox_Maximum = 1u;

static const uint Operation_AddWithDelta = 5u;
static const uint AddWithDelta_Value = 0u;
static const uint AddWithDelta_Multiplier = 1u;

static const uint Operation_Dampen = 6u;
static const uint Dampen_Drag = 0u;

static const uint Operation_LerpOverLife = 7u;
static const uint LerpOverLife_Start = 0u;
static const uint LerpOverLife_End = 1u;

static const uint Operation_KillOutsideBounds = 8u;
static const uint KillOutsideBounds_Minimum = 0u;
static const uint KillOutsideBounds_Maximum = 1u;

static const uint Operation_AddFromAttribute = 9u;
static const uint AddFromAttribute_Value = 0u;
static const uint AddFromAttribute_Scale = 1u;

static const uint Operation_SetPositionOnCircle = 10u;
static const uint SetPositionOnCircle_Center = 0u;
static const uint SetPositionOnCircle_Radius = 1u;
static const uint SetPositionOnCircle_AngularSpeed = 2u;
static const uint SetPositionOnCircle_StartAngle = 3u;

static const uint Operation_SetPositionCircularPath = 11u;
static const uint SetPositionCircularPath_BaseOffset = 0u;
static const uint SetPositionCircularPath_TimeScale = 1u;
static const uint SetPositionCircularPath_PrimaryAmplitude = 2u;
static const uint SetPositionCircularPath_SecondaryAmplitude = 3u;

static const uint Operation_SetPositionVortexRibbonPath = 12u;
static const uint SetPositionVortexRibbonPath_Center = 0u;
static const uint SetPositionVortexRibbonPath_OrbitSpeed = 1u;
static const uint SetPositionVortexRibbonPath_BaseRadius = 2u;
static const uint SetPositionVortexRibbonPath_RadiusAmplitude = 3u;
static const uint SetPositionVortexRibbonPath_RadiusSpeed = 4u;
static const uint SetPositionVortexRibbonPath_PulseAmplitude = 5u;
static const uint SetPositionVortexRibbonPath_PulseSpeed = 6u;
static const uint SetPositionVortexRibbonPath_CurlAmplitude = 7u;
static const uint SetPositionVortexRibbonPath_DepthAmplitude = 8u;

static const uint Operation_SetRibbonIdFromSpawnOrder = 13u;
static const uint SetRibbonIdFromSpawnOrder_RibbonCount = 0u;
static const uint SetRibbonIdFromSpawnOrder_FirstRibbonId = 1u;

static const uint Operation_SampleCurve = 14u;
static const uint SampleCurve_Curve = 0u;
static const uint SampleCurve_Input = 1u;

static const uint Operation_SampleColorCurve = 15u;
static const uint SampleColorCurve_Curve = 0u;
static const uint SampleColorCurve_Input = 1u;

static const uint Operation_Add = 16u;
static const uint Add_Value = 0u;

static const uint Operation_Mul = 17u;
static const uint Mul_Value = 0u;

static const uint Operation_Clamp = 18u;
static const uint Clamp_Minimum = 0u;
static const uint Clamp_Maximum = 1u;

static const uint Operation_CopyFromAttribute = 19u;
static const uint CopyFromAttribute_Value = 0u;

static const uint Operation_ApplyVortex = 20u;
static const uint ApplyVortex_Center = 0u;
static const uint ApplyVortex_Normal = 1u;
static const uint ApplyVortex_Tangential = 2u;
static const uint ApplyVortex_Radial = 3u;

#include "Operands.hlsl"

#endif
