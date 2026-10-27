#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationNotifyEvent.h>

namespace Elixir
{
    /** Complete result published only after every source and layer evaluates successfully. */
    struct ELIXIR_API SAnimationFrame
    {
        SLocalPose LocalPose;
        SModelPose ModelPose;
        std::vector<SAnimationNotifyEvent> NotifyEvents;
        SRootMotionDelta RootMotion;

        /**
         * @brief Check pose sizes and finite transforms before publication.
         * @param boneCount Number of bones required by the fixed skeleton.
         */
        bool IsValid(size_t boneCount) const;
    };
}
