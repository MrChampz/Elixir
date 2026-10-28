#include "epch.h"
#include "Pose.h"

#include <Engine/Animation/AnimationLayer.h>

namespace Elixir
{
    /* SLocalPose */

    bool SLocalPose::IsValid(const size_t boneCount) const
    {
        const auto allBonesValid = std::ranges::all_of(
            LocalTransforms,
            [](const auto& bone) { return bone.IsValid(); }
        );
        return LocalTransforms.size() == boneCount && allBonesValid;
    }

    bool SLocalPose::Blend(
        const SLocalPose& pose,
        const std::span<const float> weights,
        const float layerWeight,
        const EAnimationLayerBlendMode mode,
        const SLocalPose& reference
    )
    {
        const auto boneCount = LocalTransforms.size();

        if (!std::isfinite(layerWeight) ||
            layerWeight < 0 ||
            layerWeight > 1 ||
            boneCount != weights.size() ||
            pose.LocalTransforms.size() != boneCount ||
            (mode == EAnimationLayerBlendMode::Additive &&
                reference.LocalTransforms.size() != boneCount))
        {
            EE_CORE_ERROR("Layer composition requires one transform per skeleton bone.")
            return false;
        }

        for (size_t bone = 0; bone < boneCount; ++bone)
        {
            const auto weight = layerWeight * weights[bone];
            if (weight == 0.0f) continue;

            auto& transform = LocalTransforms[bone];
            const auto& sample = pose.LocalTransforms[bone];

            if (mode == EAnimationLayerBlendMode::Override)
            {
                transform = SBoneTransform::Blend(transform, sample, weight);
            }
            else
            {
                const auto& origin = reference.LocalTransforms[bone];

                auto rotation = sample.Rotation * glm::inverse(origin.Rotation);
                rotation = glm::normalize(rotation);
                rotation = glm::slerp(glm::identity<glm::quat>(), rotation, weight);
                rotation = glm::normalize(rotation);

                auto scale = sample.Scale / origin.Scale;
                scale = glm::mix(glm::vec3(1.0f), scale, weight);

                transform.Translation += (sample.Translation - origin.Translation) * weight;
                transform.Rotation = glm::normalize(rotation * transform.Rotation);
                transform.Scale *= scale;
            }

            if (!transform.IsValid())
            {
                EE_CORE_ERROR("Animation layer composition produced an invalid transform.")
                return false;
            }
        }

        const auto hasInfluence = std::ranges::any_of(
            weights,
            [](const float weight) { return weight > 0; }
        );
        if (layerWeight == 0 || !hasInfluence) return true;
        return Curves.ApplyLayer(
            pose.Curves,
            layerWeight,
            mode == EAnimationLayerBlendMode::Additive
        );
    }

    /* SModelPose */

    bool SModelPose::IsValid(const size_t boneCount) const
    {
        if (Matrices.size() != boneCount)
            return false;

        for (const auto& matrix : Matrices)
        {
            for (glm::length_t c = 0; c < 4; ++c)
                for (glm::length_t r = 0; r < 4; ++r)
                    if (!std::isfinite(matrix[c][r]))
                        return false;
        }

        return true;
    }
}
