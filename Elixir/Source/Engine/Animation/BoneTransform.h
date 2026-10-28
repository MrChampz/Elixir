#pragma once

#include <Engine/Core/Core.h>

#include <glm/gtc/quaternion.hpp>

namespace Elixir
{
    /** @brief Stores a bone transform relative to its parent. */
    struct ELIXIR_API SBoneTransform
    {
        static constexpr float BoneTransformTolerance = 1e-5f;

        glm::vec3 Translation{};
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale{ 1.0f };

        /**
         * @brief Interpolate local translation, scale, and shortest-path rotation.
         * @param from Source transform with a unit quaternion.
         * @param to Target transform with a unit quaternion.
         * @param weight Target weight in [0, 1].
         * @return Mixed transform; opposite scale signs can produce zero scale.
         */
        static SBoneTransform Blend(
            const SBoneTransform& from,
            const SBoneTransform& to,
            float weight
        );

        /** @brief Compose translation, rotation, and scale into a matrix. */
        glm::mat4 ToMatrix() const;

        /**
         * @brief Check whether another transform differs only within the comparison tolerance.
         * @param other Transform to compare with this transform.
         * @return True when translation, scale, and rotation are nearly equal.
         * @pre Both transforms are valid.
         */
        bool IsNearlyEqual(const SBoneTransform& other) const;

        /** @brief Check for finite values, a unit quaternion, and nonzero scale. */
        bool IsValid() const;
    };
}
