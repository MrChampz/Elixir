#include "epch.h"
#include "AnimationFrame.h"
#include "Pose.h"

namespace Elixir
{
    bool SAnimationFrame::IsValid(const size_t boneCount) const
    {
        return Sample.Pose.IsValid(boneCount) && ModelPose.IsValid(boneCount) &&
            RootMotion.IsValid();
    }
}
