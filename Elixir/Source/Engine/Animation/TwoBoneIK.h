#pragma once

#include <Engine/Animation/Skeleton.h>

namespace Elixir
{
    /** Adjusts two connected segments toward a skeleton-space target without stretching. */
    struct STwoBoneIKConstraint
    {
        /** End bone; its parent and grandparent form the two directly connected segments. */
        uint32_t EndBoneIndex = 0;

        /** Desired end position in the same skeleton space as GetModelPose(). */
        glm::vec3 TargetPosition{};

        /** Skeleton-space point toward which the middle joint bends. */
        glm::vec3 PolePosition{};

        /** Local rotation blend in [0, 1]; zero preserves the animated pose exactly. */
        float Weight = 1.0f;
    };
}
