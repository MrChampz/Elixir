#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationCurve.h>

namespace Elixir
{
    /** Stores bone transforms and scalar channels sampled together. */
    struct SAnimationSample
    {
        SLocalPose Pose;
        AnimationCurveValues Curves;
    };
}
