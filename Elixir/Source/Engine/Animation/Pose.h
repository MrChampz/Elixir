#pragma once

#include <Engine/Animation/Skeleton.h>

namespace Elixir
{
    enum class EAnimationLayerBlendMode : uint8_t;

    /**
     * Stores bone transforms relative to their parents in skeleton bone order.
     * The caller supplies the skeleton that defines the order and parent hierarchy.
     */
    struct ELIXIR_API SLocalPose
    {
        /** One transform per bone, relative to that bone's parent. */
        std::vector<SBoneTransform> LocalTransforms;

        /**
         * @brief Check that the pose contains one valid local transform per bone.
         * @param boneCount Number of bones in the skeleton used by this pose.
         * @return True when the count matches and every transform is valid.
         */
        bool IsValid(size_t boneCount) const;

        /**
         * @brief Blend another local pose into this pose.
         * @param pose Pose to apply, using the same skeleton bone order.
         * @param weights One mask weight per bone, each in [0, 1].
         * @param layerWeight Overall contribution in [0, 1], multiplied by each mask weight.
         * @param mode Override interpolation or additive blending.
         * @param reference Reference pose used to compute additive differences;
         * ignored for Override.
         * @return True on success. Failure may leave this pose partially modified.
         */
        bool Blend(
            const SLocalPose& pose,
            std::span<const float> weights,
            float layerWeight,
            EAnimationLayerBlendMode mode,
            const SLocalPose& reference = {}
        );
    };

    /** Stores bone transforms relative to the model origin, without world placement. */
    struct ELIXIR_API SModelPose
    {
        /** One accumulated matrix per bone, in skeleton bone order. */
        std::vector<glm::mat4> Matrices;

        /**
         * @brief Check that the pose contains one finite matrix per bone.
         * @param boneCount Number of bones in the skeleton used by this pose.
         * @return True when the count matches and every matrix component is finite.
         */
        bool IsValid(size_t boneCount) const;
    };
}
