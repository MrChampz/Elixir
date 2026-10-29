#pragma once

#include <Engine/Animation/AnimationSample.h>
#include <Engine/Animation/AnimationNotifyEvent.h>

namespace Elixir
{
    /**
     * Complete result shared immutably after every source and layer evaluates successfully.
     */
    struct ELIXIR_API SAnimationFrame
    {
        /** Bone transforms and scalar channels published atomically for this frame. */
        SAnimationSample Sample;
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
