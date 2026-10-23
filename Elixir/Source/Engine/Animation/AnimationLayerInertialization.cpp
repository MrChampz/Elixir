#include "epch.h"
#include "AnimationLayer.h"

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
            if (rotation.w < 0) rotation = -rotation;
            const glm::dvec3 axis(rotation.x, rotation.y, rotation.z);
            const auto length = glm::length(axis);
            return length < 1e-12 ? axis * 2.0
                : axis * (2.0 * std::atan2(length, rotation.w) / length);
        }

        /**
         * @brief Convert a rotation vector in radians to a unit quaternion.
         * @param rotation Axis scaled by its angle.
         */
        glm::dquat RotationQuaternion(const glm::dvec3& rotation)
        {
            const auto angle = glm::length(rotation);
            const auto factor = angle < 1e-12 ? 0.5 : std::sin(angle * 0.5) / angle;
            const auto axis = rotation * factor;
            return glm::normalize(glm::dquat(std::cos(angle * 0.5), axis.x, axis.y, axis.z));
        }

        /**
         * @brief Map relative angular velocity to the derivative of a rotation vector.
         * @param rotation Shortest rotation vector of the relative orientation.
         * @param velocity Angular velocity in the relative orientation's parent frame.
         */
        glm::dvec3 RotationVectorVelocity(
            const glm::dvec3& rotation, const glm::dvec3& velocity
        )
        {
            const auto angle = glm::length(rotation);
            const auto coefficient = angle < 1e-5 ? 1.0 / 12.0
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

    void AnimationLayer::ResetPoseHistory()
    {
        m_PreviousPose.clear();
        m_PreviousCurves = {};
        m_PoseDeltaSeconds = 0.0f;
    }

    bool AnimationLayer::CaptureInertialization(
        const SPlayback& playback, STransition& transition
    ) const
    {
        // A short forward probe measures destination motion without advancing its clock.
        const auto duration = GetDuration(playback);
        const auto probeSeconds = duration > 0
            ? std::min(1.0f / 120.0f,
                std::max(duration * 0.01f, std::numeric_limits<float>::denorm_min()))
            : 1.0f / 120.0f;
        std::vector<SBoneTransform> target, next;
        AnimationCurveValues targetCurves, nextCurves;
        if (!SamplePlayback(playback, target, playback.Time, &targetCurves) ||
            !SamplePlayback(playback, next, AdvancePlayback(playback, probeSeconds), &nextCurves))
            return false;
        if (target.size() != m_LocalPose.size() || next.size() != target.size())
            return false;

        const auto hasHistory = m_Playing && m_PoseDeltaSeconds > 0 &&
            m_PreviousPose.size() == target.size();
        transition.Pose = m_LocalPose;
        transition.Curves = m_Curves;
        transition.BoneOffsets.resize(target.size());
        for (size_t i = 0; i < target.size(); ++i)
        {
            const auto& source = m_LocalPose[i];
            if (!MatchingScaleSigns(source, target[i]) ||
                !MatchingScaleSigns(target[i], next[i]) ||
                (hasHistory && !MatchingScaleSigns(source, m_PreviousPose[i])))
            {
                EE_CORE_ERROR("Inertialization requires matching nonzero scale signs.")
                return false;
            }
            auto& bone = transition.BoneOffsets[i];
            bone.Translation.Value = glm::dvec3(source.Translation) - glm::dvec3(target[i].Translation);
            bone.Translation.Velocity = hasHistory
                ? (glm::dvec3(source.Translation) - glm::dvec3(m_PreviousPose[i].Translation)) /
                    double(m_PoseDeltaSeconds) : glm::dvec3(0);
            bone.Translation.Velocity -=
                (glm::dvec3(next[i].Translation) - glm::dvec3(target[i].Translation)) /
                    double(probeSeconds);

            const auto relative = glm::normalize(
                glm::dquat(source.Rotation) * glm::conjugate(glm::dquat(target[i].Rotation)));
            const auto sourceVelocity = hasHistory
                ? RotationVector(glm::dquat(source.Rotation) *
                    glm::conjugate(glm::dquat(m_PreviousPose[i].Rotation))) /
                    double(m_PoseDeltaSeconds) : glm::dvec3(0);
            const auto targetVelocity = RotationVector(glm::dquat(next[i].Rotation) *
                glm::conjugate(glm::dquat(target[i].Rotation))) / double(probeSeconds);
            bone.Rotation.Value = RotationVector(relative);
            bone.Rotation.Velocity = RotationVectorVelocity(bone.Rotation.Value,
                sourceVelocity - relative * targetVelocity);

            bone.ScaleSign = glm::sign(target[i].Scale);
            bone.Scale.Value = LogScale(source.Scale) - LogScale(target[i].Scale);
            bone.Scale.Velocity = hasHistory
                ? (LogScale(source.Scale) - LogScale(m_PreviousPose[i].Scale)) /
                    double(m_PoseDeltaSeconds) : glm::dvec3(0);
            bone.Scale.Velocity -= (LogScale(next[i].Scale) - LogScale(target[i].Scale)) /
                double(probeSeconds);
        }

        for (const auto& [name, value] : m_Curves.GetValues())
            transition.CurveOffsets.try_emplace(name);
        for (const auto& [name, value] : targetCurves.GetValues())
            transition.CurveOffsets.try_emplace(name);
        for (auto& [name, curve] : transition.CurveOffsets)
        {
            curve.Value = double(m_Curves.GetValue(name)) - targetCurves.GetValue(name);
            curve.Velocity = hasHistory
                ? (double(m_Curves.GetValue(name)) - m_PreviousCurves.GetValue(name)) /
                    double(m_PoseDeltaSeconds) : 0.0;
            curve.Velocity -= (double(nextCurves.GetValue(name)) - targetCurves.GetValue(name)) /
                double(probeSeconds);
        }
        return true;
    }

    bool AnimationLayer::ApplyInertialization(
        const double elapsed, std::vector<SBoneTransform>& pose, AnimationCurveValues& curves
    ) const
    {
        if (m_Transition.BoneOffsets.size() != pose.size() || m_Transition.Duration <= 0)
            return false;
        if (elapsed <= 0)
        {
            pose = m_Transition.Pose;
            curves = m_Transition.Curves;
            return true;
        }
        if (elapsed >= m_Transition.Duration) return true;

        const auto phase = std::clamp(elapsed / m_Transition.Duration, 0.0, 1.0);
        const auto remaining = 1.0 - phase;
        // Quintic Hermite decay: initial offset and slope, zero offset/slope/curvature at end.
        const auto valueWeight = remaining * remaining * remaining *
            (1.0 + 3.0 * phase + 6.0 * phase * phase);
        const auto velocityWeight = double(m_Transition.Duration) * phase *
            remaining * remaining * remaining * (1.0 + 3.0 * phase);
        for (size_t i = 0; i < pose.size(); ++i)
        {
            const auto& bone = m_Transition.BoneOffsets[i];
            if (glm::any(glm::notEqual(glm::sign(pose[i].Scale), bone.ScaleSign)))
            {
                EE_CORE_ERROR("Inertialization cannot cross a destination scale sign change.")
                return false;
            }
            const auto translation = glm::dvec3(pose[i].Translation) +
                bone.Translation.Value * valueWeight + bone.Translation.Velocity * velocityWeight;
            const auto scale = glm::dvec3(bone.ScaleSign) * glm::exp(
                LogScale(pose[i].Scale) + bone.Scale.Value * valueWeight +
                    bone.Scale.Velocity * velocityWeight);
            if (!FitsFloat(translation) || !FitsFloat(scale) ||
                glm::any(glm::lessThan(glm::abs(scale), glm::dvec3(1e-8))))
            {
                EE_CORE_ERROR("Inertialization exceeds the supported transform range.")
                return false;
            }
            pose[i].Translation = glm::vec3(translation);
            const auto rotation = RotationQuaternion(
                bone.Rotation.Value * valueWeight + bone.Rotation.Velocity * velocityWeight);
            pose[i].Rotation = glm::quat(glm::normalize(rotation * glm::dquat(pose[i].Rotation)));
            pose[i].Scale = glm::vec3(scale);
            if (!pose[i].IsValid())
            {
                EE_CORE_ERROR("Inertialization produced an invalid local transform.")
                return false;
            }
        }
        for (const auto& [name, curve] : m_Transition.CurveOffsets)
        {
            const auto value = double(curves.GetValue(name)) + curve.Value * valueWeight +
                curve.Velocity * velocityWeight;
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
            {
                EE_CORE_ERROR("Inertialization exceeds the supported scalar curve range.")
                return false;
            }
            if (!curves.SetValue(name, float(value))) return false;
        }
        return true;
    }
}
