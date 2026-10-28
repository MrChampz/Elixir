#pragma once

#include <Engine/Animation/AnimationPlayback.h>
#include <Engine/Animation/Pose.h>

namespace Elixir
{
    /** Cross-fades playback or a frozen pose into a separately owned destination. */
    class ELIXIR_API AnimationTransition
    {
    public:
        /** Outgoing playback that continues advancing during the transition. */
        AnimationPlayback Playback;

        /** Captured source pose used when there is no outgoing playback. */
        SLocalPose Pose;

        /** Scalar channels captured with a frozen transition source pose. */
        AnimationCurveValues Curves;

        /** Finite nonnegative transition duration in seconds; zero disables the fade. */
        float Duration = 0;

        /** Elapsed transition time in seconds, clamped to [0, Duration]. */
        float Elapsed = 0;

        /**
         * @brief Sample the target and blend its outgoing contribution.
         * @param playback Target playback.
         * @param source Outgoing playback, if assigned.
         * @param weight Target contribution in [0, 1].
         * @param time Target normalized phase.
         * @param sourceTime Outgoing normalized phase.
         * @param output Receives the blended local pose.
         * @param referencePose Mesh-specific sampling defaults.
         */
        bool Sample(
            const AnimationPlayback& playback,
            const AnimationPlayback& source,
            float weight,
            double time,
            double sourceTime,
            SLocalPose& output,
            const SLocalPose& referencePose
        ) const;

        /**
         * @brief Sample and blend the target pose and scalar channels together.
         * @param playback Target playback.
         * @param source Outgoing playback, if assigned.
         * @param weight Target contribution in [0, 1].
         * @param time Target normalized phase.
         * @param sourceTime Outgoing normalized phase.
         * @param output Receives the blended pose and scalar channels.
         * @param referencePose Mesh-specific sampling defaults for bone channels.
         */
        bool Sample(
            const AnimationPlayback& playback,
            const AnimationPlayback& source,
            float weight,
            double time,
            double sourceTime,
            SAnimationEvaluation& output,
            const SLocalPose& referencePose
        ) const;

        /**
         * @brief Extract root movement from playback and blend it with the outgoing transition.
         *
         * Use the mean fade weight over the interval. This approximation can vary with
         * frame partition when root velocity changes or translation combines with rotation.
         *
         * @param playback Target playback whose movement contributes to the result.
         * @param interval Root reference transform, bone index, and interval to extract.
         * @param output Receives the blended movement on success; unchanged on failure.
         * @return True if movement was extracted successfully.
         */
        bool ExtractRootMotion(
            const AnimationPlayback& playback,
            const SRootMotionIntervalContext& interval,
            SRootMotionDelta& output
        ) const;

        /** @brief Check whether the transition contributes an outgoing pose. */
        bool IsActive() const { return Duration > 0; }

        /** @brief Validate timing and the exclusive live or frozen transition origin. */
        bool IsValid() const;
    };
}
