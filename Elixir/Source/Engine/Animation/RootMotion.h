#pragma once

#include <Engine/Animation/Skeleton.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Elixir
{
    /** Rigid movement in mesh-local space, applied after the character's current transform. */
    struct SRootMotionDelta
    {
        /** Translation in the mesh's authored units. Scale is not extracted. */
        glm::vec3 Translation{};

        /** Unit rotation relative to the character's current orientation. */
        glm::quat Rotation{ 1, 0, 0, 0 };

        /** @brief Check for finite translation and a unit rotation. */
        bool IsValid() const
        {
            return SBoneTransform{ Translation, Rotation, glm::vec3(1) }.IsValid();
        }

        /** @brief Build the rigid matrix to post-multiply onto the character transform. */
        glm::mat4 ToMatrix() const
        {
            return glm::translate(glm::mat4(1), Translation) * glm::mat4_cast(Rotation);
        }

        /** @brief Get the movement that reverses this delta. */
        SRootMotionDelta Inverse() const
        {
            const auto inverse = glm::conjugate(Rotation);
            return { inverse * -Translation, inverse };
        }

        /**
         * @brief Compose another movement after this delta.
         * @param next Movement in the coordinate frame reached by this delta.
         */
        SRootMotionDelta operator*(const SRootMotionDelta& next) const
        {
            return {
                Translation + Rotation * next.Translation,
                glm::normalize(Rotation * next.Rotation)
            };
        }

        /**
         * @brief Blend translation and shortest-path rotation between movement deltas.
         * @param from Source movement.
         * @param to Target movement.
         * @param weight Target contribution in [0, 1].
         */
        static SRootMotionDelta Blend(
            const SRootMotionDelta& from,
            const SRootMotionDelta& to,
            const float weight
        )
        {
            return {
                glm::mix(from.Translation, to.Translation, weight),
                glm::normalize(glm::slerp(from.Rotation, to.Rotation, weight))
            };
        }
    };
}
