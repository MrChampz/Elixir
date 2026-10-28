#pragma once

#include <Engine/Animation/BoneTransform.h>

namespace Elixir
{
    struct SModelPose;

    /** Defines an attachment point relative to one skeleton bone. */
    struct ELIXIR_API SSkeletonSocket
    {
        /** Unique nonempty name within the skeleton. */
        std::string Name;

        /** Index in Skeleton::GetBones(), independent of mesh skin palettes. */
        uint32_t BoneIndex = 0;

        /** Offset in the bone's local space, including attachment rotation and scale. */
        SBoneTransform RelativeTransform;

        /**
         * @brief Compose the bone matrix and socket offset into a skeleton-space transform.
         *
         * Multiply the result by the character world transform to place an attachment.
         * No inverse bind matrices are applied and animation is not advanced.
         *
         * @param modelPose Final model-space pose using the owning skeleton's bone order.
         * @return Empty for a missing bone, invalid offset, or nonfinite matrix or result.
         */
        std::optional<glm::mat4> GetTransform(const SModelPose& modelPose) const;
    };
}
