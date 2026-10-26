#include "epch.h"
#include "AnimationFrame.h"
#include "Pose.h"

namespace Elixir
{
    bool SAnimationFrame::IsValid(const size_t boneCount) const
    {
        return LocalPose.IsValid(boneCount) && ModelPose.IsValid(boneCount) &&
            RootMotion.IsValid();
    }
}
