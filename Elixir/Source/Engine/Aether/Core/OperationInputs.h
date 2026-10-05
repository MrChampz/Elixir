#pragma once

/** @brief Defines argument positions in compiled particle operations. */
namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for SetLiteral. */
    inline constexpr uint32_t SetLiteral_Value = 0;

    /** @brief Position of the Minimum argument for RandomRange. */
    inline constexpr uint32_t RandomRange_Minimum = 0;
    /** @brief Position of the Maximum argument for RandomRange. */
    inline constexpr uint32_t RandomRange_Maximum = 1;

    /** @brief Position of the Center argument for SampleDisk. */
    inline constexpr uint32_t SampleDisk_Center = 0;
    /** @brief Position of the Radius argument for SampleDisk. */
    inline constexpr uint32_t SampleDisk_Radius = 1;
    /** @brief Position of the Normal argument for SampleDisk. */
    inline constexpr uint32_t SampleDisk_Normal = 2;

    /** @brief Position of the Direction argument for SampleCone. */
    inline constexpr uint32_t SampleCone_Direction = 0;
    /** @brief Position of the Angle argument for SampleCone. */
    inline constexpr uint32_t SampleCone_Angle = 1;
    /** @brief Position of the MinSpeed argument for SampleCone. */
    inline constexpr uint32_t SampleCone_MinSpeed = 2;
    /** @brief Position of the MaxSpeed argument for SampleCone. */
    inline constexpr uint32_t SampleCone_MaxSpeed = 3;

    /** @brief Position of the Minimum argument for SampleBox. */
    inline constexpr uint32_t SampleBox_Minimum = 0;
    /** @brief Position of the Maximum argument for SampleBox. */
    inline constexpr uint32_t SampleBox_Maximum = 1;

    /** @brief Position of the Value argument for AddWithDelta. */
    inline constexpr uint32_t AddWithDelta_Value = 0;
    /** @brief Position of the Multiplier argument for AddWithDelta. */
    inline constexpr uint32_t AddWithDelta_Multiplier = 1;

    /** @brief Position of the Drag argument for Dampen. */
    inline constexpr uint32_t Dampen_Drag = 0;

    /** @brief Position of the Start argument for LerpOverLife. */
    inline constexpr uint32_t LerpOverLife_Start = 0;
    /** @brief Position of the End argument for LerpOverLife. */
    inline constexpr uint32_t LerpOverLife_End = 1;

    /** @brief Position of the Minimum argument for KillOutsideBounds. */
    inline constexpr uint32_t KillOutsideBounds_Minimum = 0;
    /** @brief Position of the Maximum argument for KillOutsideBounds. */
    inline constexpr uint32_t KillOutsideBounds_Maximum = 1;

    /** @brief Position of the Value argument for AddFromAttribute. */
    inline constexpr uint32_t AddFromAttribute_Value = 0;
    /** @brief Position of the Scale argument for AddFromAttribute. */
    inline constexpr uint32_t AddFromAttribute_Scale = 1;

    /** @brief Position of the Center argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_Center = 0;
    /** @brief Position of the Radius argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_Radius = 1;
    /** @brief Position of the AngularSpeed argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_AngularSpeed = 2;
    /** @brief Position of the StartAngle argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_StartAngle = 3;

    /** @brief Position of the BaseOffset argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_BaseOffset = 0;
    /** @brief Position of the TimeScale argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_TimeScale = 1;
    /** @brief Position of the PrimaryAmplitude argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_PrimaryAmplitude = 2;
    /** @brief Position of the SecondaryAmplitude argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_SecondaryAmplitude = 3;

    /** @brief Position of the Center argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_Center = 0;
    /** @brief Position of the OrbitSpeed argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_OrbitSpeed = 1;
    /** @brief Position of the BaseRadius argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_BaseRadius = 2;
    /** @brief Position of the RadiusAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_RadiusAmplitude = 3;
    /** @brief Position of the RadiusSpeed argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_RadiusSpeed = 4;
    /** @brief Position of the PulseAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_PulseAmplitude = 5;
    /** @brief Position of the PulseSpeed argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_PulseSpeed = 6;
    /** @brief Position of the CurlAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_CurlAmplitude = 7;
    /** @brief Position of the DepthAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_DepthAmplitude = 8;

    /** @brief Position of the RibbonCount argument for SetRibbonIdFromSpawnOrder. */
    inline constexpr uint32_t SetRibbonIdFromSpawnOrder_RibbonCount = 0;
    /** @brief Position of the FirstRibbonId argument for SetRibbonIdFromSpawnOrder. */
    inline constexpr uint32_t SetRibbonIdFromSpawnOrder_FirstRibbonId = 1;

    /** @brief Position of the Curve argument for SampleCurve. */
    inline constexpr uint32_t SampleCurve_Curve = 0;
    /** @brief Position of the Input argument for SampleCurve. */
    inline constexpr uint32_t SampleCurve_Input = 1;

    /** @brief Position of the Curve argument for SampleColorCurve. */
    inline constexpr uint32_t SampleColorCurve_Curve = 0;
    /** @brief Position of the Input argument for SampleColorCurve. */
    inline constexpr uint32_t SampleColorCurve_Input = 1;

    /** @brief Position of the Value argument for Add. */
    inline constexpr uint32_t Add_Value = 0;

    /** @brief Position of the Value argument for Mul. */
    inline constexpr uint32_t Mul_Value = 0;

    /** @brief Position of the Minimum argument for Clamp. */
    inline constexpr uint32_t Clamp_Minimum = 0;
    /** @brief Position of the Maximum argument for Clamp. */
    inline constexpr uint32_t Clamp_Maximum = 1;

    /** @brief Position of the Value argument for CopyFromAttribute. */
    inline constexpr uint32_t CopyFromAttribute_Value = 0;

    /** @brief Position of the Center argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Center = 0;
    /** @brief Position of the Normal argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Normal = 1;
    /** @brief Position of the Tangential argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Tangential = 2;
    /** @brief Position of the Radial argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Radial = 3;
}
