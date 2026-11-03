#include "epch.h"
#include "AnimationInertialization.h"

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

    AnimationInertialization::SInertialVector AnimationInertialization::CaptureVectorOffsets(
        const glm::dvec3& source,
        const glm::dvec3& previous,
        const glm::dvec3& target,
        const glm::dvec3& next,
        const double sourceSeconds,
        const double targetSeconds
    )
    {
        const auto sourceVelocity = sourceSeconds > 0
            ? (source - previous) / sourceSeconds
            : glm::dvec3(0);

        const auto targetVelocity = (next - target) / targetSeconds;

        return {
            .Offset = source - target,
            .OffsetVelocity = sourceVelocity - targetVelocity
        };
    }

    AnimationInertialization::SInertialVector AnimationInertialization::CaptureRotationOffsets(
        const glm::dquat& source,
        const glm::dquat& previous,
        const glm::dquat& target,
        const glm::dquat& next,
        const double sourceSeconds,
        const double targetSeconds
    )
    {
        const auto sourceDelta = source * glm::conjugate(previous);
        const auto targetDelta = next * glm::conjugate(target);

        const auto sourceVelocity = sourceSeconds > 0
            ? RotationVector(sourceDelta) / sourceSeconds
            : glm::dvec3(0);

        const auto targetVelocity = RotationVector(targetDelta) / targetSeconds;

        const auto relative = glm::normalize(source * glm::conjugate(target));
        const auto offset = RotationVector(relative);
        const auto relativeVelocity = sourceVelocity - relative * targetVelocity;

        return {
            .Offset = offset,
            .OffsetVelocity = RotationVectorVelocity(offset, relativeVelocity)
        };
    }

    bool AnimationInertialization::Capture(
        const SAnimationSample& sourceSample,
        const SAnimationSample* previous,
        const float deltaSeconds,
        const SAnimationSample& targetSample,
        const SAnimationSample& nextSample,
        const float probeSeconds
    )
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0 ||
            !std::isfinite(probeSeconds) || probeSeconds <= 0)
        {
            EE_CORE_ERROR("Inertialization requires finite intervals and a positive probe.")
            return false;
        }

        const auto& target = targetSample.Pose.LocalTransforms;
        const auto& next = nextSample.Pose.LocalTransforms;

        if (target.empty() ||
            !sourceSample.Pose.IsValid(target.size()) ||
            !targetSample.Pose.IsValid(target.size()) ||
            !nextSample.Pose.IsValid(target.size()))
        {
            EE_CORE_ERROR("Inertialization requires matching valid sample poses.")
            return false;
        }

        const auto hasHistory = previous && deltaSeconds > 0 &&
            previous->Pose.LocalTransforms.size() == target.size();

        if (hasHistory && !previous->Pose.IsValid(target.size()))
        {
            EE_CORE_ERROR("Inertialization requires valid source history.")
            return false;
        }

        std::vector<SBoneInertia> boneOffsets(target.size());
        std::map<std::string, SInertialScalar, std::less<>> curveOffsets;

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

            auto& bone = boneOffsets[i];

            const auto& prior = hasHistory ? previous->Pose.LocalTransforms[i] : source;
            const auto sourceSeconds = hasHistory ? double(deltaSeconds) : 0.0;

            const auto& destination = target[i];
            const auto& probe = next[i];

            bone.Translation = CaptureVectorOffsets(
                glm::dvec3(source.Translation),
                glm::dvec3(prior.Translation),
                glm::dvec3(destination.Translation),
                glm::dvec3(probe.Translation),
                sourceSeconds,
                double(probeSeconds)
            );

            bone.Rotation = CaptureRotationOffsets(
                glm::dquat(source.Rotation),
                glm::dquat(prior.Rotation),
                glm::dquat(destination.Rotation),
                glm::dquat(probe.Rotation),
                sourceSeconds,
                double(probeSeconds)
            );

            bone.Scale = CaptureVectorOffsets(
                LogScale(source.Scale),
                LogScale(prior.Scale),
                LogScale(destination.Scale),
                LogScale(probe.Scale),
                sourceSeconds,
                double(probeSeconds)
            );

            bone.ScaleSign = glm::sign(destination.Scale);
        }

        for (const auto& name : sourceSample.Curves.GetValues() | std::views::keys)
            curveOffsets.try_emplace(name);

        for (const auto& name : targetSample.Curves.GetValues() | std::views::keys)
            curveOffsets.try_emplace(name);

        if (hasHistory)
        {
            for (const auto& name : previous->Curves.GetValues() | std::views::keys)
                curveOffsets.try_emplace(name);
        }

        for (auto& [name, curve] : curveOffsets)
        {
            const auto sourceValue = double(sourceSample.Curves.GetValue(name));
            const auto targetValue = double(targetSample.Curves.GetValue(name));
            const auto nextValue = double(nextSample.Curves.GetValue(name));

            double sourceVelocity = 0;

            if (hasHistory)
            {
                const auto previousValue = double(previous->Curves.GetValue(name));
                sourceVelocity = (sourceValue - previousValue) / double(deltaSeconds);
            }

            const auto targetVelocity = (nextValue - targetValue) / double(probeSeconds);

            curve.Offset = sourceValue - targetValue;
            curve.OffsetVelocity = sourceVelocity - targetVelocity;
        }

        m_SourceSample = sourceSample;
        m_BoneOffsets = std::move(boneOffsets);
        m_CurveOffsets = std::move(curveOffsets);

        return true;
    }

    bool AnimationInertialization::Apply(
        const double elapsed,
        const float duration,
        SAnimationSample& sample
    ) const
    {
        auto& pose = sample.Pose.LocalTransforms;
        auto& curves = sample.Curves;

        if (!IsCaptured() ||
            !std::isfinite(elapsed) || elapsed < 0 ||
            !std::isfinite(duration) || duration <= 0 ||
            m_BoneOffsets.size() != pose.size())
            return false;

        if (elapsed <= 0)
        {
            sample = m_SourceSample;
            return true;
        }

        if (elapsed >= duration)
            return true;

        const auto phase = std::clamp(elapsed / duration, 0.0, 1.0);
        const auto phaseSquared = phase * phase;
        const auto remaining = 1.0 - phase;
        const auto remainingCubed = remaining * remaining * remaining;

        // Quintic Hermite decay ends with zero offset, velocity, and acceleration.
        const auto offsetWeight = remainingCubed * (1.0 + 3.0 * phase + 6.0 * phaseSquared);
        const auto velocityWeight =
            double(duration) * phase * remainingCubed * (1.0 + 3.0 * phase);

        for (size_t i = 0; i < pose.size(); ++i)
        {
            if (!ApplyBoneOffsets(m_BoneOffsets[i], offsetWeight, velocityWeight, pose[i]))
                return false;
        }

        return ApplyCurveOffsets(offsetWeight, velocityWeight, curves);
    }

    bool AnimationInertialization::ApplyBoneOffsets(
        const SBoneInertia& bone,
        const double offsetWeight,
        const double velocityWeight,
        SBoneTransform& transform
    )
    {
        const auto scaleSigns = glm::sign(transform.Scale);
        const auto scaleSignsChanged = glm::any(glm::notEqual(scaleSigns, bone.ScaleSign));

        if (scaleSignsChanged)
        {
            EE_CORE_ERROR("Inertialization cannot cross a destination scale sign change.")
            return false;
        }

        const auto translationOffset = bone.Translation.Offset * offsetWeight;
        const auto translationMotion = bone.Translation.OffsetVelocity * velocityWeight;
        const auto translation =
            glm::dvec3(transform.Translation) + translationOffset + translationMotion;

        const auto scaleOffset = bone.Scale.Offset * offsetWeight;
        const auto scaleMotion = bone.Scale.OffsetVelocity * velocityWeight;
        const auto logScale = LogScale(transform.Scale) + scaleOffset + scaleMotion;
        const auto scaleMagnitude = glm::exp(logScale);
        const auto scale = glm::dvec3(bone.ScaleSign) * scaleMagnitude;
        const auto scaleTooSmall = glm::any(glm::lessThan(glm::abs(scale), glm::dvec3(1e-8)));

        if (!FitsFloat(translation) || !FitsFloat(scale) || scaleTooSmall)
        {
            EE_CORE_ERROR("Inertialization exceeds the supported transform range.")
            return false;
        }

        const auto rotationOffset = bone.Rotation.Offset * offsetWeight;
        const auto rotationMotion = bone.Rotation.OffsetVelocity * velocityWeight;
        const auto rotationCorrection = RotationQuaternion(rotationOffset + rotationMotion);
        const auto rotation = rotationCorrection * glm::dquat(transform.Rotation);

        transform.Translation = glm::vec3(translation);
        transform.Rotation = glm::quat(glm::normalize(rotation));
        transform.Scale = glm::vec3(scale);

        if (!transform.IsValid())
        {
            EE_CORE_ERROR("Inertialization produced an invalid local transform.")
            return false;
        }

        return true;
    }

    bool AnimationInertialization::ApplyCurveOffsets(
        const double offsetWeight,
        const double velocityWeight,
        AnimationCurveValues& curves
    ) const
    {
        for (const auto& [name, curve] : m_CurveOffsets)
        {
            const auto destinationValue = double(curves.GetValue(name));
            const auto offset = curve.Offset * offsetWeight;
            const auto motion = curve.OffsetVelocity * velocityWeight;
            const auto value = destinationValue + offset + motion;

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
