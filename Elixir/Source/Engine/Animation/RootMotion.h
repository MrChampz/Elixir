#pragma once

#include <Engine/Animation/Skeleton.h>

namespace Elixir
{
    class AnimationPlayback;
    class AnimationTransition;
    class AnimationMontagePlayback;
    struct SAnimationMontageEvaluation;
    struct SAnimationNodeState;

    /** Rigid movement in mesh-local space, applied after the character's current transform. */
    struct SRootMotionDelta
    {
        /** Translation in the mesh's authored units. Scale is not extracted. */
        glm::vec3 Translation{};

        /** Unit rotation relative to the character's current orientation. */
        glm::quat Rotation{ 1, 0, 0, 0 };

        /** @brief Get the movement that reverses this delta. */
        SRootMotionDelta Inverse() const
        {
            const auto inverse = glm::conjugate(Rotation);
            return { inverse * -Translation, inverse };
        }

        /** @brief Build the rigid matrix to post-multiply onto the character transform. */
        glm::mat4 ToMatrix() const
        {
            return glm::translate(glm::mat4(1), Translation) * glm::mat4_cast(Rotation);
        }

        /** @brief Check for finite translation and a unit rotation. */
        bool IsValid() const
        {
            const auto transform = SBoneTransform{ Translation, Rotation, glm::vec3(1) };
            return transform.IsValid();
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

    /** Describes the root bone and the playback interval used to extract movement. */
    struct SRootMotionIntervalContext
    {
        /** Root bone's local reference transform, borrowed for this extraction. */
        const SBoneTransform& Reference;

        /** Index of the root bone in skeleton order. */
        uint32_t BoneIndex = 0;

        /** Finite nonnegative interval length in seconds. */
        float DeltaSeconds = 0;

        /** Finite nonnegative offset from the frame's start, in seconds. */
        float OffsetSeconds = 0;
    };

    /** Resolved source and action inputs required to extract one layer's movement. */
    struct SRootMotionEvaluationContext
    {
        const AnimationPlayback& Playback;
        const AnimationTransition& Transition;
        const AnimationMontagePlayback& Montage;
        const SAnimationMontageEvaluation& MontageEvaluation;
        const SRootMotionIntervalContext& Interval;
        bool Playing = false;
    };

    /** Source data and clock interval consumed by root-motion extraction. */
    struct SAnimationMotionContext
    {
        /** Resolved timing and source data, borrowed for this extraction. */
        const SAnimationNodeState& State;

        /** Root reference transform, bone index, duration, and offset to extract. */
        const SRootMotionIntervalContext& Interval;

        /**
         * Finite normalized playback position at the frame's start, before the interval
         * offset.
         */
        double Phase = 0;

        /** Allow the extraction interval to cross cycle boundaries. */
        bool Looping = true;
    };

    namespace RootMotion
    {
        /**
         * @brief Extract movement from resolved contributions or composed child frames.
         * @param context Resolved sources and requested playback interval.
         * @param output Receives the blended root movement.
         */
        ELIXIR_API bool Extract(
            const SAnimationMotionContext& context,
            SRootMotionDelta& output
        );

        /**
         * @brief Combine continuing playback and visited action movement intervals.
         * @param context Validated sources and their evaluated montage intervals.
         * @param output Receives the complete layer movement delta.
         */
        ELIXIR_API bool Extract(
            const SRootMotionEvaluationContext& context,
            SRootMotionDelta& output
        );
    }
}
