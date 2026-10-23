#include "epch.h"
#include "SkeletalMeshInstance.h"

namespace Elixir
{
    namespace
    {
        /** Identifies a validated bone frame with an optional socket offset. */
        struct SIKReference
        {
            uint32_t BoneIndex;
            const SSkeletonSocket* Socket = nullptr;
        };

        /** Stores one resolved target in skeleton space for the current solve. */
        struct SIKTarget
        {
            glm::dvec3 Position;
            std::optional<glm::dquat> Rotation;
        };

        /**
         * @brief Find a target frame in the instance's fixed skeleton.
         * @param skeleton Hierarchy and sockets defining the reference.
         * @param reference Bone index or socket name.
         */
        std::optional<SIKReference> FindTargetReference(
            const Skeleton& skeleton, const TIKTargetReference& reference
        )
        {
            if (const auto* bone = std::get_if<uint32_t>(&reference))
            {
                if (*bone < skeleton.GetBones().size()) return SIKReference{*bone};
            }
            else if (const auto* name = std::get_if<std::string>(&reference))
            {
                const auto index = skeleton.FindSocket(*name);
                if (index >= 0)
                {
                    const auto& socket = skeleton.GetSockets()[size_t(index)];
                    return SIKReference{socket.BoneIndex, &socket};
                }
            }
            EE_CORE_ERROR("IK target reference must identify an existing bone or socket.")
            return std::nullopt;
        }

        /**
         * @brief Check whether scale preserves an unambiguous reference rotation.
         * @param scale Local scale in the target reference hierarchy or socket.
         */
        bool HasUniformPositiveScale(const glm::vec3& scale)
        {
            const auto tolerance = 1e-5f * std::abs(scale.x);
            return scale.x > 0 && std::abs(scale.x - scale.y) <= tolerance &&
                std::abs(scale.x - scale.z) <= tolerance;
        }

        /**
         * @brief Resolve a target from the current candidate pose without using prior frames.
         * @param skeleton Hierarchy and sockets defining reference indices.
         * @param constraint Target frame and local offsets.
         * @param local Current parent-relative transforms.
         * @param model Current skeleton-space matrices, including earlier constraints.
         */
        std::optional<SIKTarget> ResolveTarget(
            const Skeleton& skeleton,
            const STwoBoneIKConstraint& constraint,
            const std::span<const SBoneTransform> local,
            const std::span<const glm::mat4> model
        )
        {
            SIKTarget result{glm::dvec3(constraint.TargetPosition)};
            if (constraint.TargetRotation)
                result.Rotation = glm::normalize(glm::dquat(*constraint.TargetRotation));
            if (!constraint.TargetReference) return result;

            const auto reference = FindTargetReference(skeleton, *constraint.TargetReference);
            if (!reference) return std::nullopt;
            auto frame = glm::dmat4(model[reference->BoneIndex]);
            if (reference->Socket)
                frame *= glm::dmat4(reference->Socket->RelativeTransform.ToMatrix());
            result.Position = glm::dvec3(frame * glm::dvec4(result.Position, 1));
            for (int i = 0; i < 3; ++i)
            {
                if (!std::isfinite(result.Position[i]))
                {
                    EE_CORE_ERROR("IK target reference produced a nonfinite position.")
                    return std::nullopt;
                }
            }
            if (!result.Rotation) return result;

            glm::dquat rotation(1, 0, 0, 0);
            for (auto bone = int32_t(reference->BoneIndex); bone >= 0;
                bone = skeleton.GetBones()[size_t(bone)].ParentIndex)
            {
                if (!HasUniformPositiveScale(local[size_t(bone)].Scale))
                {
                    EE_CORE_ERROR("IK rotation references require uniform positive ancestor scales.")
                    return std::nullopt;
                }
                rotation = glm::dquat(local[size_t(bone)].Rotation) * rotation;
            }
            if (reference->Socket)
            {
                const auto& offset = reference->Socket->RelativeTransform;
                if (!HasUniformPositiveScale(offset.Scale))
                {
                    EE_CORE_ERROR("IK rotation references require uniform positive socket scale.")
                    return std::nullopt;
                }
                rotation *= glm::dquat(offset.Rotation);
            }
            result.Rotation = glm::normalize(rotation * *result.Rotation);
            return result;
        }

        /**
         * @brief Choose a stable unit vector perpendicular to a unit direction.
         * @param direction Unit segment direction.
         */
        glm::dvec3 Perpendicular(const glm::dvec3& direction)
        {
            const auto axis = std::abs(direction.z) < 0.9
                ? glm::dvec3(0, 0, 1) : glm::dvec3(0, 1, 0);
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
         * @param constraint Target, pole, optional orientation, and weight.
         */
        bool HasValidSettings(const STwoBoneIKConstraint& constraint)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!std::isfinite(constraint.TargetPosition[i]) ||
                    !std::isfinite(constraint.PolePosition[i]))
                    return false;
            }
            if ((constraint.WeightCurve && constraint.WeightCurve->empty()) ||
                !std::isfinite(constraint.Weight) ||
                constraint.Weight < 0 || constraint.Weight > 1)
                return false;

            if (constraint.TargetRotation)
            {
                const auto& rotation = *constraint.TargetRotation;
                for (int i = 0; i < 4; ++i)
                    if (!std::isfinite(rotation[i])) return false;
                if (std::abs(glm::dot(rotation, rotation) - 1.0f) >= 1e-4f)
                    return false;
            }
            return true;
        }
    }

    bool SkeletalMeshInstance::SetTwoBoneIKConstraints(
        const std::span<const STwoBoneIKConstraint> constraints
    )
    {
        if (!constraints.empty() && !m_Mesh)
        {
            EE_CORE_ERROR("Two-bone IK requires a skeletal mesh.")
            return false;
        }
        for (const auto& constraint : constraints)
        {
            if (!HasValidSettings(constraint))
            {
                EE_CORE_ERROR("Two-bone IK requires finite targets, a unit target rotation, and weight in [0, 1].")
                return false;
            }
            const auto& bones = m_Mesh->GetSkeleton()->GetBones();
            if (constraint.EndBoneIndex >= bones.size() ||
                bones[constraint.EndBoneIndex].ParentIndex < 0 ||
                bones[size_t(bones[constraint.EndBoneIndex].ParentIndex)].ParentIndex < 0)
            {
                EE_CORE_ERROR("Two-bone IK requires an end bone with two direct ancestors.")
                return false;
            }
        }
        for (size_t i = 0; i < constraints.size(); ++i)
        {
            const auto& constraint = constraints[i];
            if (!constraint.TargetReference) continue;
            const auto& skeleton = *m_Mesh->GetSkeleton();
            const auto reference = FindTargetReference(skeleton, *constraint.TargetReference);
            if (!reference) return false;
            if (constraint.Weight == 0) continue;

            // A reference must be stable after its follower is solved.
            for (size_t j = i; j < constraints.size(); ++j)
            {
                if (constraints[j].Weight == 0) continue;
                const auto& bones = skeleton.GetBones();
                const auto middle = bones[constraints[j].EndBoneIndex].ParentIndex;
                const auto root = bones[size_t(middle)].ParentIndex;
                for (auto bone = int32_t(reference->BoneIndex); bone >= 0;
                    bone = bones[size_t(bone)].ParentIndex)
                {
                    if (bone == root)
                    {
                        EE_CORE_ERROR("IK target references must precede their followers and avoid feedback.")
                        return false;
                    }
                }
            }
        }
        // Preserve overlapping input; ordinary per-frame changes reuse existing capacity.
        for (const auto& existing : m_TwoBoneIKConstraints)
        {
            if (constraints.data() == &existing)
            {
                std::vector replacement(constraints.begin(), constraints.end());
                m_TwoBoneIKConstraints = std::move(replacement);
                return true;
            }
        }
        m_TwoBoneIKConstraints.assign(constraints.begin(), constraints.end());
        return true;
    }

    bool SkeletalMeshInstance::ApplyTwoBoneIK(
        std::vector<SBoneTransform>& local,
        std::vector<glm::mat4>& model,
        const AnimationCurveValues& curves
    ) const
    {
        const auto& skeleton = *m_Mesh->GetSkeleton();
        const auto& bones = skeleton.GetBones();
        for (const auto& constraint : m_TwoBoneIKConstraints)
        {
            const auto weight = constraint.Weight * (constraint.WeightCurve
                ? std::clamp(curves.GetValue(*constraint.WeightCurve), 0.0f, 1.0f) : 1.0f);
            if (weight == 0) continue;

            const auto targetFrame = ResolveTarget(skeleton, constraint, local, model);
            if (!targetFrame) return false;
            const auto end = constraint.EndBoneIndex;
            const auto middle = uint32_t(bones[end].ParentIndex);
            const auto root = uint32_t(bones[middle].ParentIndex);
            if (m_RootMotionEnabled)
            {
                for (auto bone = int32_t(m_RootMotionBone); bone >= 0;
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
                const auto& scale = local[size_t(bone)].Scale;
                const auto tolerance = 1e-5f * std::abs(scale.x);
                if (scale.x <= 0 || std::abs(scale.x - scale.y) > tolerance ||
                    std::abs(scale.x - scale.z) > tolerance)
                {
                    EE_CORE_ERROR("Two-bone IK requires uniform positive scales through its ancestors.")
                    return false;
                }
                if (uint32_t(bone) != root)
                    parentRotation = glm::dquat(local[size_t(bone)].Rotation) * parentRotation;
            }
            const auto& middleScale = local[middle].Scale;
            const auto tolerance = 1e-5f * std::abs(middleScale.x);
            if (middleScale.x <= 0 || std::abs(middleScale.x - middleScale.y) > tolerance ||
                std::abs(middleScale.x - middleScale.z) > tolerance)
            {
                EE_CORE_ERROR("Two-bone IK requires uniform positive scales on both segments.")
                return false;
            }

            const glm::dvec3 start(model[root][3]);
            const glm::dvec3 joint(model[middle][3]);
            const glm::dvec3 tip(model[end][3]);
            const auto firstLength = glm::length(joint - start);
            const auto secondLength = glm::length(tip - joint);
            if (firstLength < 1e-8 || secondLength < 1e-8)
            {
                EE_CORE_ERROR("Two-bone IK requires nonzero segment lengths.")
                return false;
            }

            const auto target = targetFrame->Position - start;
            const auto targetDistance = glm::length(target);
            const auto currentDirection = tip - start;
            const auto currentDistance = glm::length(currentDirection);
            const auto forward = targetDistance > 1e-12 ? target / targetDistance :
                currentDistance > 1e-12 ? currentDirection / currentDistance :
                    (joint - start) / firstLength;
            auto bend = glm::dvec3(constraint.PolePosition) - start;
            bend -= forward * glm::dot(bend, forward);
            if (glm::length(bend) < 1e-8)
            {
                bend = joint - start;
                bend -= forward * glm::dot(bend, forward);
            }
            bend = glm::length(bend) < 1e-8 ? Perpendicular(forward) : glm::normalize(bend);

            const auto distance = glm::clamp(targetDistance,
                std::abs(firstLength - secondLength), firstLength + secondLength);
            const auto along = distance > 1e-12
                ? (firstLength * firstLength - secondLength * secondLength + distance * distance) /
                    (2.0 * distance)
                : 0.0;
            const auto height = std::sqrt(std::max(0.0, firstLength * firstLength - along * along));
            const auto desiredFirst = forward * along + bend * height;
            const auto desiredSecond = forward * distance - desiredFirst;
            const auto rootDelta = RotationBetween((joint - start) / firstLength,
                glm::normalize(desiredFirst));
            const auto oldRoot = glm::normalize(parentRotation * glm::dquat(local[root].Rotation));
            const auto newRoot = glm::normalize(rootDelta * oldRoot);
            const auto oldMiddle = glm::normalize(oldRoot * glm::dquat(local[middle].Rotation));
            const auto middleDelta = RotationBetween(rootDelta * ((tip - joint) / secondLength),
                glm::normalize(desiredSecond));
            const auto newMiddle = glm::normalize(middleDelta * rootDelta * oldMiddle);
            const auto rootLocal = glm::quat(glm::normalize(glm::conjugate(parentRotation) * newRoot));
            const auto middleLocal = glm::quat(glm::normalize(glm::conjugate(newRoot) * newMiddle));
            local[root].Rotation = glm::normalize(glm::slerp(
                local[root].Rotation, rootLocal, weight));
            local[middle].Rotation = glm::normalize(glm::slerp(
                local[middle].Rotation, middleLocal, weight));
            if (targetFrame->Rotation)
            {
                const auto blendedRoot = glm::normalize(
                    parentRotation * glm::dquat(local[root].Rotation));
                const auto blendedMiddle = glm::normalize(
                    blendedRoot * glm::dquat(local[middle].Rotation));
                const auto currentEnd = glm::normalize(
                    blendedMiddle * glm::dquat(local[end].Rotation));
                const auto targetEnd = glm::normalize(glm::slerp(currentEnd,
                    *targetFrame->Rotation,
                    double(weight)));
                local[end].Rotation = glm::quat(glm::normalize(
                    glm::conjugate(blendedMiddle) * targetEnd));
            }
            if (!skeleton.BuildModelPose(local, model)) return false;
        }
        return true;
    }
}
