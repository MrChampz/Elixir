#pragma once

#include <Engine/Animation/Skeleton.h>

#include <variant>

namespace Elixir
{
    /** Selects a bone index or socket name in the instance's own skeleton. */
    using TIKTargetReference = std::variant<uint32_t, std::string>;

    /** Adjusts two connected segments toward a skeleton-space target without stretching. */
    struct STwoBoneIKConstraint
    {
        /** End bone; its parent and grandparent form the two directly connected segments. */
        uint32_t EndBoneIndex = 0;

        /** End position in skeleton space, or a local offset when TargetReference is set. */
        glm::vec3 TargetPosition{};

        /** Skeleton-space point toward which the middle joint bends. */
        glm::vec3 PolePosition{};

        /** Rotation blend in [0, 1]; zero preserves the animated pose exactly. */
        float Weight = 1.0f;

        /**
         * Optional unit rotation in skeleton space or the selected reference frame.
         * Absent preserves the end bone's local rotation.
         */
        std::optional<glm::quat> TargetRotation;

        /**
         * Optional bone index or socket name defining the target frame. Absent uses
         * skeleton space. Position and rotation become offsets in this frame; the pole
         * remains in skeleton space. Read from the current pose at this constraint's turn.
         */
        std::optional<TIKTargetReference> TargetReference;

        /**
         * Optional nonempty curve name multiplying Weight in the same evaluated frame.
         * The sampled value clamps to [0, 1]; an absent curve contributes zero. Dependency
         * validation uses the configured Weight because curves can change every frame.
         */
        std::optional<std::string> WeightCurve;

    };
}
