#include "epch.h"
#include "AnimationTransition.h"

#include <limits>

namespace Elixir
{
    namespace
    {
        /**
         * @brief Check that a vector can be stored in finite float components.
         * @param value Double-precision candidate before narrowing.
         */
        bool FitsFloat(const glm::dvec3& value)
        {
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(value[i]) ||
                    std::abs(value[i]) > std::numeric_limits<float>::max())
                    return false;
            return true;
        }

        /**
         * @brief Convert a rotation to a shortest-path rotation vector in radians.
         * @param rotation Unit quaternion whose sign does not affect its orientation.
         */
        glm::dvec3 RotationVector(glm::dquat rotation)
        {
            rotation = glm::normalize(rotation);

            if (rotation.w < 0)
                rotation = -rotation;

            const glm::dvec3 axis(rotation.x, rotation.y, rotation.z);
            const auto length = glm::length(axis);

            return length < 1e-12
                ? axis * 2.0
                : axis * (2.0 * std::atan2(length, rotation.w) / length);
        }

        /**
         * @brief Convert a rotation vector in radians to a unit quaternion.
         * @param rotation Axis scaled by its angle.
         */
        glm::dquat RotationQuaternion(const glm::dvec3& rotation)
        {
            const auto angle = glm::length(rotation);
            const auto factor = angle < 1e-12
                ? 0.5
                : std::sin(angle * 0.5) / angle;

            const auto axis = rotation * factor;
            return glm::normalize(glm::dquat(std::cos(angle * 0.5), axis.x, axis.y, axis.z));
        }

        /**
         * @brief Map relative angular velocity to the derivative of a rotation vector.
         * @param rotation Shortest rotation vector of the relative orientation.
         * @param velocity Angular velocity in the relative orientation's parent frame.
         */
        glm::dvec3 RotationVectorVelocity(
            const glm::dvec3& rotation,
            const glm::dvec3& velocity
        )
        {
            const auto angle = glm::length(rotation);
            const auto coefficient = angle < 1e-5
                ? 1.0 / 12.0
                : (1.0 - 0.5 * angle / std::tan(0.5 * angle)) / (angle * angle);

            return velocity - 0.5 * glm::cross(rotation, velocity) +
                   coefficient * glm::cross(rotation, glm::cross(rotation, velocity));
        }

        /**
         * @brief Get logarithmic scale magnitudes without changing reflection signs.
         * @param scale Finite nonzero scale components.
         */
        glm::dvec3 LogScale(const glm::vec3& scale)
        {
            return glm::log(glm::abs(glm::dvec3(scale)));
        }

        /**
         * @brief Check whether two local transforms have matching scale signs.
         * @param first First valid local transform.
         * @param second Second valid local transform.
         */
        bool MatchingScaleSigns(const SBoneTransform& first, const SBoneTransform& second)
        {
            return glm::all(glm::equal(glm::sign(first.Scale), glm::sign(second.Scale)));
        }
    }

    bool AnimationTransition::CaptureInertialization(
        AnimationPlayback& playback,
        const SAnimationSample& sourceSample,
        const SAnimationSample* previous,
        const float deltaSeconds,
        const SLocalPose& referencePose
    )
    {
        SAnimationSample targetSample, nextSample;

        // Resolve entry transitions before measuring motion, and retain that state for playback.
        auto destination = playback;

        const auto initialPhase = destination.Advance(0, referencePose);

        if (!destination.Sample(initialPhase, referencePose, targetSample))
            return false;

        destination.Apply(initialPhase);

        // A short forward probe measures destination motion without advancing its clock.
        const auto duration = destination.GetDuration();

        auto probe = destination;
        const auto probeSeconds = duration > 0
            ? std::min(
                  1.0f / 120.0f,
                  std::max(duration * 0.01f, std::numeric_limits<float>::denorm_min())
              )
            : 1.0f / 120.0f;

        const auto nextPhase = probe.Advance(probeSeconds, referencePose);
        if (!probe.Sample(nextPhase, referencePose, nextSample))
            return false;

        const auto& target = targetSample.Pose.LocalTransforms;
        const auto& next = nextSample.Pose.LocalTransforms;

        if (target.size() != sourceSample.Pose.LocalTransforms.size() ||
            next.size() != target.size())
            return false;

        const auto hasHistory = previous && deltaSeconds > 0 &&
            previous->Pose.LocalTransforms.size() == target.size();

        SourceSample = sourceSample;

        m_BoneOffsets.resize(target.size());

        for (size_t i = 0; i < target.size(); ++i)
        {
            const auto& source = sourceSample.Pose.LocalTransforms[i];

            if (!MatchingScaleSigns(source, target[i]) ||
                !MatchingScaleSigns(target[i], next[i]) ||
                (hasHistory &&
                    !MatchingScaleSigns(source, previous->Pose.LocalTransforms[i])))
            {
                EE_CORE_ERROR("Inertialization requires matching nonzero scale signs.")
                return false;
            }

            auto& bone = m_BoneOffsets[i];

            bone.Translation.Value =
                glm::dvec3(source.Translation) - glm::dvec3(target[i].Translation);
            bone.Translation.Velocity = hasHistory
                ? (glm::dvec3(source.Translation) -
                  glm::dvec3(previous->Pose.LocalTransforms[i].Translation)) /
                     double(deltaSeconds)
                : glm::dvec3(0);
            bone.Translation.Velocity -=
                (glm::dvec3(next[i].Translation) - glm::dvec3(target[i].Translation)) /
                double(probeSeconds);

            const auto relative = glm::normalize(
                glm::dquat(source.Rotation) * glm::conjugate(glm::dquat(target[i].Rotation))
            );

            const auto sourceVelocity = hasHistory
                ? RotationVector(
                     glm::dquat(source.Rotation) *
                     glm::conjugate(
                         glm::dquat(previous->Pose.LocalTransforms[i].Rotation)
                     )
                  ) / double(deltaSeconds)
                : glm::dvec3(0);

            const auto targetVelocity = RotationVector(
                glm::dquat(next[i].Rotation) *
                glm::conjugate(glm::dquat(target[i].Rotation))
            ) / double(probeSeconds);

            bone.Rotation.Value = RotationVector(relative);
            bone.Rotation.Velocity = RotationVectorVelocity(
                bone.Rotation.Value, sourceVelocity - relative * targetVelocity
            );

            bone.ScaleSign = glm::sign(target[i].Scale);
            bone.Scale.Value = LogScale(source.Scale) - LogScale(target[i].Scale);
            bone.Scale.Velocity = hasHistory
                ? (LogScale(source.Scale) -
                 LogScale(previous->Pose.LocalTransforms[i].Scale)) / double(deltaSeconds)
                : glm::dvec3(0);
            bone.Scale.Velocity -=
                (LogScale(next[i].Scale) - LogScale(target[i].Scale)) / double(probeSeconds);
        }

        for (const auto& name : sourceSample.Curves.GetValues() | std::views::keys)
            m_CurveOffsets.try_emplace(name);

        for (const auto& name : targetSample.Curves.GetValues() | std::views::keys)
            m_CurveOffsets.try_emplace(name);

        for (auto& [name, curve] : m_CurveOffsets)
        {
            curve.Value = double(sourceSample.Curves.GetValue(name)) -
                          targetSample.Curves.GetValue(name);
            curve.Velocity = hasHistory
                ? (double(sourceSample.Curves.GetValue(name)) -
                    previous->Curves.GetValue(name)) / double(deltaSeconds)
                : 0.0;
            curve.Velocity -= (double(nextSample.Curves.GetValue(name)) -
                targetSample.Curves.GetValue(name)) / double(probeSeconds);
        }

        playback = std::move(destination);

        return true;
    }

    bool AnimationTransition::ApplyInertialization(
        const double elapsed,
        SAnimationSample& sample
    ) const
    {
        auto& pose = sample.Pose.LocalTransforms;
        auto& curves = sample.Curves;

        if (m_BoneOffsets.size() != pose.size() || Duration <= 0)
            return false;

        if (elapsed <= 0)
        {
            sample = SourceSample;
            return true;
        }

        if (elapsed >= Duration)
            return true;

        const auto phase = std::clamp(elapsed / Duration, 0.0, 1.0);
        const auto remaining = 1.0 - phase;

        // Quintic Hermite decay: initial offset and slope, zero offset/slope/curvature at end.
        const auto valueWeight =
            remaining * remaining * remaining * (1.0 + 3.0 * phase + 6.0 * phase * phase);

        const auto velocityWeight =
            double(Duration) * phase * remaining * remaining * remaining * (1.0 + 3.0 * phase);

        for (size_t i = 0; i < pose.size(); ++i)
        {
            const auto& bone = m_BoneOffsets[i];

            if (glm::any(glm::notEqual(glm::sign(pose[i].Scale), bone.ScaleSign)))
            {
                EE_CORE_ERROR("Inertialization cannot cross a destination scale sign change.")
                return false;
            }

            const auto translation = glm::dvec3(pose[i].Translation) +
                bone.Translation.Value * valueWeight +
                bone.Translation.Velocity * velocityWeight;
            const auto scale = glm::dvec3(bone.ScaleSign) *
                glm::exp(
                    LogScale(pose[i].Scale) + bone.Scale.Value * valueWeight +
                    bone.Scale.Velocity * velocityWeight
                );

            if (!FitsFloat(translation) ||
                !FitsFloat(scale) ||
                glm::any(glm::lessThan(glm::abs(scale), glm::dvec3(1e-8))))
            {
                EE_CORE_ERROR("Inertialization exceeds the supported transform range.")
                return false;
            }

            const auto rotation = RotationQuaternion(
                bone.Rotation.Value * valueWeight + bone.Rotation.Velocity * velocityWeight
            );

            pose[i].Translation = glm::vec3(translation);
            pose[i].Rotation =
                glm::quat(glm::normalize(rotation * glm::dquat(pose[i].Rotation)));
            pose[i].Scale = glm::vec3(scale);

            if (!pose[i].IsValid())
            {
                EE_CORE_ERROR("Inertialization produced an invalid local transform.")
                return false;
            }
        }

        for (const auto& [name, curve] : m_CurveOffsets)
        {
            const auto value = double(curves.GetValue(name)) +
                curve.Value * valueWeight + curve.Velocity * velocityWeight;

            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
            {
                EE_CORE_ERROR("Inertialization exceeds the supported scalar curve range.")
                return false;
            }

            if (!curves.SetValue(name, float(value)))
                return false;
        }
        
        return true;
    }
}
