#include "epch.h"
#include "TwoBoneIK.h"

#include <cmath>

namespace Elixir
{
    namespace
    {
        /**
         * @brief Choose a stable unit vector perpendicular to a unit direction.
         * @param direction Unit segment direction.
         */
        glm::dvec3 Perpendicular(const glm::dvec3& direction)
        {
            const auto axis = std::abs(direction.z) < 0.9
                ? glm::dvec3(0, 0, 1)
                : glm::dvec3(0, 1, 0);

            return glm::normalize(glm::cross(direction, axis));
        }

        /**
         * @brief Rotate one unit vector onto another, including opposite directions.
         * @param from Source unit vector.
         * @param to Destination unit vector.
         */
        glm::dquat RotationBetween(const glm::dvec3& from, const glm::dvec3& to)
        {
            const auto cosine = glm::clamp(glm::dot(from, to), -1.0, 1.0);
            if (cosine < -1.0 + 1e-12)
                return glm::dquat(0, Perpendicular(from));

            return glm::normalize(glm::dquat(1.0 + cosine, glm::cross(from, to)));
        }

        /**
         * @brief Check skeleton-space target and blend settings without changing the pose.
         * @param constraint Requested target, pole, and weight.
         */
        bool HasValidSettings(const STwoBoneIKConstraint& constraint)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!std::isfinite(constraint.TargetPosition[i]) ||
                    !std::isfinite(constraint.PolePosition[i]))
                    return false;
            }
            return std::isfinite(constraint.Weight) &&
                constraint.Weight >= 0 &&
                constraint.Weight <= 1;
        }
    }

    Ref<TwoBoneIK> TwoBoneIK::Create(Ref<const Skeleton> skeleton)
    {
        if (!skeleton)
        {
            EE_CORE_ERROR("Two-bone IK requires a skeleton.")
            return nullptr;
        }

        return Ref<TwoBoneIK>(new TwoBoneIK(std::move(skeleton)));
    }

    bool TwoBoneIK::SetConstraints(const std::span<const STwoBoneIKConstraint> constraints)
    {
        if (!constraints.empty() && !m_Skeleton)
        {
            EE_CORE_ERROR("Two-bone IK requires a skeleton.")
            return false;
        }

        for (const auto& constraint : constraints)
        {
            if (!HasValidSettings(constraint))
            {
                EE_CORE_ERROR("Two-bone IK requires finite targets and a weight in [0, 1].")
                return false;
            }

            const auto& bones = m_Skeleton->GetBones();

            if (constraint.EndBoneIndex >= bones.size() ||
                bones[constraint.EndBoneIndex].ParentIndex < 0 ||
                bones[size_t(bones[constraint.EndBoneIndex].ParentIndex)].ParentIndex < 0)
            {
                EE_CORE_ERROR("Two-bone IK requires an end bone with two direct ancestors.")
                return false;
            }
        }

        // Preserve overlapping input; ordinary per-frame changes reuse existing capacity.
        for (const auto& existing : m_Constraints)
        {
            if (constraints.data() == &existing)
            {
                std::vector replacement(constraints.begin(), constraints.end());
                m_Constraints = std::move(replacement);
                return true;
            }
        }

        m_Constraints.assign(constraints.begin(), constraints.end());
        return true;
    }

    bool TwoBoneIK::Apply(const SPoseModifierContext& context, SLocalPose& pose) const
    {
        const auto& skeleton = context.Rig;
        const auto& bones = skeleton.GetBones();

        if (!m_Skeleton->IsCompatible(skeleton) ||
            !pose.IsValid(bones.size()) ||
            (context.RootMotionBone && *context.RootMotionBone >= bones.size()))
        {
            EE_CORE_ERROR("Two-bone IK requires a compatible rig and valid candidate pose.")
            return false;
        }

        SModelPose model;
        if (!skeleton.BuildModelPose(pose, model))
            return false;

        auto& transforms = pose.LocalTransforms;
        const auto& matrices = model.Matrices;

        for (const auto& constraint : m_Constraints)
        {
            if (constraint.Weight == 0) continue;

            const auto end = constraint.EndBoneIndex;
            const auto middle = uint32_t(bones[end].ParentIndex);
            const auto root = uint32_t(bones[middle].ParentIndex);

            if (context.RootMotionBone)
            {
                for (auto bone = int32_t(*context.RootMotionBone);
                    bone >= 0;
                    bone = bones[size_t(bone)].ParentIndex)
                {
                    if (uint32_t(bone) == root || uint32_t(bone) == middle)
                    {
                        EE_CORE_ERROR("Two-bone IK cannot rotate the root-motion bone or its ancestors.")
                        return false;
                    }
                }
            }

            glm::dquat parentRotation(1, 0, 0, 0);

            for (auto bone = int32_t(root); bone >= 0; bone = bones[size_t(bone)].ParentIndex)
            {
                const auto& scale = transforms[size_t(bone)].Scale;
                const auto tolerance = 1e-5f * std::abs(scale.x);

                if (scale.x <= 0 ||
                    std::abs(scale.x - scale.y) > tolerance ||
                    std::abs(scale.x - scale.z) > tolerance)
                {
                    EE_CORE_ERROR("Two-bone IK requires uniform positive scales through its ancestors.")
                    return false;
                }

                if (uint32_t(bone) != root)
                {
                    parentRotation =
                        glm::dquat(transforms[size_t(bone)].Rotation) * parentRotation;
                }
            }

            const auto& middleScale = transforms[middle].Scale;
            const auto tolerance = 1e-5f * std::abs(middleScale.x);

            if (middleScale.x <= 0 ||
                std::abs(middleScale.x - middleScale.y) > tolerance ||
                std::abs(middleScale.x - middleScale.z) > tolerance)
            {
                EE_CORE_ERROR("Two-bone IK requires uniform positive scales on both segments.")
                return false;
            }

            const glm::dvec3 start(matrices[root][3]);
            const glm::dvec3 joint(matrices[middle][3]);
            const glm::dvec3 tip(matrices[end][3]);

            const auto firstLength = glm::length(joint - start);
            const auto secondLength = glm::length(tip - joint);

            if (firstLength < 1e-8 || secondLength < 1e-8)
            {
                EE_CORE_ERROR("Two-bone IK requires nonzero segment lengths.")
                return false;
            }

            const auto target = glm::dvec3(constraint.TargetPosition) - start;
            const auto targetDistance = glm::length(target);
            const auto currentDirection = tip - start;
            const auto currentDistance = glm::length(currentDirection);

            const auto forward = targetDistance > 1e-12
                ? target / targetDistance
                : currentDistance > 1e-12
                    ? currentDirection / currentDistance
                    : (joint - start) / firstLength;

            auto bend = glm::dvec3(constraint.PolePosition) - start;
            bend -= forward * glm::dot(bend, forward);

            if (glm::length(bend) < 1e-8)
            {
                bend = joint - start;
                bend -= forward * glm::dot(bend, forward);
            }

            bend = glm::length(bend) < 1e-8
                ? Perpendicular(forward)
                : glm::normalize(bend);

            const auto distance = glm::clamp(
                targetDistance,
                std::abs(firstLength - secondLength),
                firstLength + secondLength
            );

            const auto along = distance > 1e-12
                ? (firstLength * firstLength - secondLength * secondLength +
                        distance * distance) / (2.0 * distance)
                : 0.0;

            const auto heightSquared = std::max(
                0.0,
                firstLength * firstLength - along * along
            );
            const auto height = std::sqrt(heightSquared);

            const auto desiredFirst = forward * along + bend * height;
            const auto desiredSecond = forward * distance - desiredFirst;

            const auto rootDelta = RotationBetween(
                (joint - start) / firstLength,
                glm::normalize(desiredFirst)
            );

            const auto oldRoot =
                glm::normalize(parentRotation * glm::dquat(transforms[root].Rotation));
            const auto newRoot = glm::normalize(rootDelta * oldRoot);

            const auto oldMiddle =
                glm::normalize(oldRoot * glm::dquat(transforms[middle].Rotation));
            const auto middleDelta = RotationBetween(
                rootDelta * ((tip - joint) / secondLength),
                glm::normalize(desiredSecond)
            );
            const auto newMiddle = glm::normalize(middleDelta * rootDelta * oldMiddle);

            const auto rootLocal =
                glm::quat(glm::normalize(glm::conjugate(parentRotation) * newRoot));
            const auto middleLocal =
                glm::quat(glm::normalize(glm::conjugate(newRoot) * newMiddle));

            transforms[root].Rotation = glm::normalize(
                glm::slerp(
                    transforms[root].Rotation,
                    rootLocal,
                    constraint.Weight
                )
            );

            transforms[middle].Rotation = glm::normalize(
                glm::slerp(
                    transforms[middle].Rotation,
                    middleLocal,
                    constraint.Weight
                )
            );

            if (!skeleton.BuildModelPose(pose, model))
                return false;
        }

        return true;
    }

    TwoBoneIK::TwoBoneIK(Ref<const Skeleton> skeleton)
      : m_Skeleton(std::move(skeleton)) {}
}
