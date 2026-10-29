#pragma once

#include <Engine/Animation/AnimationSample.h>

namespace Elixir
{
    /** Captures sample differences and decays them while preserving initial motion. */
    class ELIXIR_API AnimationInertialization
    {
    public:
        /**
         * @brief Capture pose, curve, and velocity offsets from sampled animation data.
         * @param sourceSample Last accepted source sample.
         * @param previous Previous source sample; ignored if null or its bone count differs.
         * @param deltaSeconds Finite nonnegative source interval; zero disables history.
         * @param targetSample Destination sample at the transition start.
         * @param nextSample Destination sample after the probe interval.
         * @param probeSeconds Finite positive seconds between destination samples.
         * @return True on success. Preserve the previous capture on failure.
         */
        bool Capture(
            const SAnimationSample& sourceSample,
            const SAnimationSample* previous,
            float deltaSeconds,
            const SAnimationSample& targetSample,
            const SAnimationSample& nextSample,
            float probeSeconds
        );

        /**
         * @brief Decay captured offsets over a destination sample.
         * @param elapsed Finite nonnegative seconds since the transition started.
         * @param duration Finite positive transition duration in seconds.
         * @param sample Destination sample to adjust; discard it on failure.
         * @return True when captured offsets were applied successfully.
         */
        bool Apply(double elapsed, float duration, SAnimationSample& sample) const;

        /** @brief Check whether source offsets have been captured successfully. */
        bool IsCaptured() const { return !m_SourceSample.Pose.LocalTransforms.empty(); }

    private:
        struct SInertialVector
        {
            glm::dvec3 Offset{};
            glm::dvec3 OffsetVelocity{};
        };

        struct SInertialScalar
        {
            double Offset = 0;
            double OffsetVelocity = 0;
        };

        struct SBoneInertia
        {
            SInertialVector Translation;
            SInertialVector Rotation;
            SInertialVector Scale;
            glm::vec3 ScaleSign{1};
        };

        /**
         * @brief Capture offsets for translation or logarithmic scale vectors.
         * @param source Current source value.
         * @param previous Previous source value; ignored when sourceSeconds is zero.
         * @param target Initial destination value.
         * @param next Probed destination value.
         * @param sourceSeconds Source sample interval; zero disables source velocity.
         * @param targetSeconds Positive destination probe interval.
         */
        static SInertialVector CaptureVectorOffsets(
            const glm::dvec3& source,
            const glm::dvec3& previous,
            const glm::dvec3& target,
            const glm::dvec3& next,
            double sourceSeconds,
            double targetSeconds
        );

        /**
         * @brief Capture shortest rotation offsets and their angular motion.
         * @param source Current source rotation.
         * @param previous Previous source rotation; ignored when sourceSeconds is zero.
         * @param target Initial destination rotation.
         * @param next Probed destination rotation.
         * @param sourceSeconds Source sample interval; zero disables source velocity.
         * @param targetSeconds Positive destination probe interval.
         */
        static SInertialVector CaptureRotationOffsets(
            const glm::dquat& source,
            const glm::dquat& previous,
            const glm::dquat& target,
            const glm::dquat& next,
            double sourceSeconds,
            double targetSeconds
        );

        /**
         * @brief Apply decayed offsets to one destination bone transform.
         * @param bone Captured offsets and scale signs for this bone.
         * @param offsetWeight Decay weight for the initial offset.
         * @param velocityWeight Decay weight for the initial offset velocity.
         * @param transform Destination transform to adjust; discard it on failure.
         */
        static bool ApplyBoneOffsets(
            const SBoneInertia& bone,
            double offsetWeight,
            double velocityWeight,
            SBoneTransform& transform
        );

        /**
         * @brief Apply decayed offsets to destination scalar curves.
         * @param offsetWeight Decay weight for the initial offset.
         * @param velocityWeight Decay weight for the initial offset velocity.
         * @param curves Destination curves to adjust; discard them on failure.
         */
        bool ApplyCurveOffsets(
            double offsetWeight,
            double velocityWeight,
            AnimationCurveValues& curves
        ) const;

        /** Exact source sample returned before any transition time has elapsed. */
        SAnimationSample m_SourceSample;
        std::vector<SBoneInertia> m_BoneOffsets;
        std::map<std::string, SInertialScalar, std::less<>> m_CurveOffsets;
    };
}
