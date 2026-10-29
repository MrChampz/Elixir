#pragma once

#include <Engine/Animation/AnimationPlayback.h>
#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationTransitionMode.h>

namespace Elixir
{
    /** Smooths source changes by cross-fading or decaying captured motion offsets. */
    class ELIXIR_API AnimationTransition
    {
    public:
        /** Outgoing playback that continues advancing during the transition. */
        AnimationPlayback Playback;

        /** Captured pose and scalar channels used when there is no outgoing playback. */
        SAnimationSample SourceSample;

        /** Finite nonnegative transition duration in seconds; zero disables the fade. */
        float Duration = 0;

        /** Elapsed transition time in seconds, clamped to [0, Duration]. */
        float Elapsed = 0;

        /** Inertialization releases the outgoing source and uses destination root motion. */
        EAnimationTransitionMode Mode = EAnimationTransitionMode::CrossFade;

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
            SAnimationSample& output,
            const SLocalPose& referencePose
        ) const;

        /**
         * @brief Capture pose and velocity offsets against a new destination.
         * @param playback Destination playback; receives its resolved entry state on success.
         * @param sourceSample Last accepted source sample before montage and composition.
         * @param previous Previous accepted source sample, or null when unavailable.
         * @param deltaSeconds Seconds between accepted samples; zero disables velocity
         * history.
         * @param referencePose Defaults for unanimated destination bone channels.
         */
        bool CaptureInertialization(
            AnimationPlayback& playback,
            const SAnimationSample& sourceSample,
            const SAnimationSample* previous,
            float deltaSeconds,
            const SLocalPose& referencePose
        );

        /**
         * @brief Extract root movement from playback and blend it with the outgoing transition.
         *
         * Inertialization uses destination movement immediately.
         * CrossFade uses the mean fade weight over the interval. This approximation can vary
         * with frame partition when root velocity changes or translation combines with
         * rotation.
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
    private:
        struct SInertialVector
        {
            glm::dvec3 Value{};
            glm::dvec3 Velocity{};
        };

        struct SInertialScalar
        {
            double Value = 0;
            double Velocity = 0;
        };

        struct SBoneInertia
        {
            SInertialVector Translation;
            SInertialVector Rotation;
            SInertialVector Scale;
            glm::vec3 ScaleSign{1};
        };

        /**
         * @brief Decay captured offsets over a candidate destination sample.
         * @param elapsed Transition seconds clamped to the configured duration.
         * @param sample Candidate pose and curves; discard on failure.
         */
        bool ApplyInertialization(double elapsed, SAnimationSample& sample) const;

        std::vector<SBoneInertia> m_BoneOffsets;
        std::map<std::string, SInertialScalar, std::less<>> m_CurveOffsets;
    };
}
