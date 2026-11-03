#include "epch.h"
#include "BoneTransform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Elixir
{
    /* SBoneTransform */

    SBoneTransform SBoneTransform::Blend(
        const SBoneTransform& from,
        const SBoneTransform& to,
        const float weight
    )
    {
        return {
            .Translation = glm::mix(from.Translation, to.Translation, weight),
            .Rotation = glm::normalize(glm::slerp(from.Rotation, to.Rotation, weight)),
            .Scale = glm::mix(from.Scale, to.Scale, weight),
        };
    }

    glm::mat4 SBoneTransform::ToMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), Translation) *
            glm::mat4_cast(Rotation) *
            glm::scale(glm::mat4(1.0f), Scale);
    }

    bool SBoneTransform::IsNearlyEqual(const SBoneTransform& other) const
    {
        constexpr auto tolerance = glm::vec3(BoneTransformTolerance);

        const auto translationDelta = glm::abs(Translation - other.Translation);
        const auto scaleDelta = glm::abs(Scale - other.Scale);
        const auto rotationAlignment = std::abs(glm::dot(Rotation, other.Rotation));

        const auto translationChanged =
            glm::any(glm::greaterThan(translationDelta, tolerance));
        const auto scaleChanged = glm::any(glm::greaterThan(scaleDelta, tolerance));
        const auto rotationChanged = rotationAlignment < 1.0f - BoneTransformTolerance;

        return !translationChanged && !scaleChanged && !rotationChanged;
    }

    bool SBoneTransform::IsValid() const
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!std::isfinite(Translation[i]) ||
                !std::isfinite(Scale[i]) ||
                std::abs(Scale[i]) < 1e-8f)
                return false;
        }

        for (int i = 0; i < 4; ++i)
        {
            if (!std::isfinite(Rotation[i]))
                return false;
        }

        return std::abs(glm::dot(Rotation, Rotation) - 1.0f) < 1e-4f;
    }
}
