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
         * @brief Check an optional target orientation for finite unit-quaternion components.
         * @param target Optional skeleton-space orientation.
         */
        bool HasValidTargetRotation(const std::optional<glm::quat>& target)
        {
            if (!target) return true;

            const auto& rotation = *target;

            for (int i = 0; i < 4; ++i)
                if (!std::isfinite(rotation[i]))
                    return false;

            return std::abs(glm::dot(rotation, rotation) - 1.0f) < 1e-4f;
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
            return std::isfinite(constraint.Weight) &&
                constraint.Weight >= 0 &&
                constraint.Weight <= 1 &&
                HasValidTargetRotation(constraint.TargetRotation);
        }
        /** Identifies the two segments and their accumulated parent orientation. */
        struct SIKChain
        {
            uint32_t Root;
            uint32_t Middle;
            uint32_t End;
            glm::dquat ParentRotation{1, 0, 0, 0};
        };

        /** Stores current joint positions and segment lengths in skeleton-space units. */
        struct SChainGeometry
        {
            glm::dvec3 Start;
            glm::dvec3 Joint;
            glm::dvec3 Tip;
            double FirstLength;
            double SecondLength;
        };

        /** Stores reachable segment directions with their original lengths. */
        struct SDesiredSegments
        {
            glm::dvec3 First;
            glm::dvec3 Second;
        };

        /** Stores solved parent-relative rotations before applying the constraint weight. */
        struct SLocalRotations
        {
            glm::quat Root;
            glm::quat Middle;
        };

        /**
         * @brief Validate one constraint without replacing the configured list.
         * @param constraint Requested target, pole, weight, and end bone.
         * @param skeleton Rig whose direct ancestors define the chain.
         * @return False for invalid settings or an end bone without two direct ancestors.
         */
        bool ValidateConstraint(
            const STwoBoneIKConstraint& constraint,
            const Skeleton& skeleton
        )
        {
            if (!HasValidSettings(constraint))
            {
                EE_CORE_ERROR(
                    "Two-bone IK requires finite targets, a unit target rotation, "
                    "and weight in [0, 1]."
                )
                return false;
            }

            const auto& bones = skeleton.GetBones();

            if (constraint.EndBoneIndex >= bones.size() ||
                bones[constraint.EndBoneIndex].ParentIndex < 0 ||
                bones[size_t(bones[constraint.EndBoneIndex].ParentIndex)].ParentIndex < 0)
            {
                EE_CORE_ERROR("Two-bone IK requires an end bone with two direct ancestors.")
                return false;
            }

            return true;
        }

        /**
         * @brief Check whether a scale supports rigid two-segment solving.
         * @param scale Finite local scale of an active chain bone or ancestor.
         */
        bool HasUniformPositiveScale(const glm::vec3& scale)
        {
            const auto tolerance = 1e-5f * std::abs(scale.x);
            return scale.x > 0 &&
                std::abs(scale.x - scale.y) <= tolerance &&
                std::abs(scale.x - scale.z) <= tolerance;
        }

        /**
         * @brief Check that solving the chain cannot rotate the extracted movement frame.
         * @param context Rig and optional extracted root-motion bone.
         * @param chain The two parent bones that the solver would rotate.
         * @return False when either writable bone is the extracted root or an ancestor.
         */
        bool PreservesRootMotion(const SPoseModifierContext& context, const SIKChain& chain)
        {
            if (!context.RootMotionBone) return true;

            const auto& bones = context.Rig.GetBones();

            for (auto bone = int32_t(*context.RootMotionBone);
                bone >= 0;
                bone = bones[size_t(bone)].ParentIndex)
            {
                if (uint32_t(bone) == chain.Root || uint32_t(bone) == chain.Middle)
                {
                    EE_CORE_ERROR("Two-bone IK cannot rotate the root-motion bone or its ancestors.")
                    return false;
                }
            }

            return true;
        }

        /**
         * @brief Resolve the chain and validate scales and the protected root frame.
         * @param context Validated rig and optional extracted root-motion bone.
         * @param constraint Previously validated constraint defining the end bone.
         * @param pose Current candidate transforms, including earlier constraints.
         * @param chain Receives bone indices and accumulated parent orientation.
         * @return False if the chain changes the extracted root frame or uses invalid scales.
         */
        bool PrepareChain(
            const SPoseModifierContext& context,
            const STwoBoneIKConstraint& constraint,
            const SLocalPose& pose,
            SIKChain& chain
        )
        {
            const auto& bones = context.Rig.GetBones();
            const auto& transforms = pose.LocalTransforms;

            chain.End = constraint.EndBoneIndex;
            chain.Middle = uint32_t(bones[chain.End].ParentIndex);
            chain.Root = uint32_t(bones[chain.Middle].ParentIndex);

            if (!PreservesRootMotion(context, chain))
                return false;

            for (auto bone = int32_t(chain.Root);
                bone >= 0;
                bone = bones[size_t(bone)].ParentIndex)
            {
                if (!HasUniformPositiveScale(transforms[size_t(bone)].Scale))
                {
                    EE_CORE_ERROR("Two-bone IK requires uniform positive scales through its ancestors.")
                    return false;
                }

                if (uint32_t(bone) != chain.Root)
                {
                    chain.ParentRotation =
                        glm::dquat(transforms[size_t(bone)].Rotation) * chain.ParentRotation;
                }
            }

            if (!HasUniformPositiveScale(transforms[chain.Middle].Scale))
            {
                EE_CORE_ERROR("Two-bone IK requires uniform positive scales on both segments.")
                return false;
            }

            return true;
        }

        /**
         * @brief Measure the current skeleton-space chain without changing its pose.
         * @param chain Bone indices resolved for the current constraint.
         * @param model Current accumulated matrices, including earlier constraints.
         * @param geometry Receives joint positions and segment lengths.
         * @return False when either segment is shorter than the supported minimum.
         */
        bool MeasureChain(
            const SIKChain& chain,
            const SModelPose& model,
            SChainGeometry& geometry
        )
        {
            geometry.Start = glm::dvec3(model.Matrices[chain.Root][3]);
            geometry.Joint = glm::dvec3(model.Matrices[chain.Middle][3]);
            geometry.Tip = glm::dvec3(model.Matrices[chain.End][3]);
            geometry.FirstLength = glm::length(geometry.Joint - geometry.Start);
            geometry.SecondLength = glm::length(geometry.Tip - geometry.Joint);

            if (geometry.FirstLength < 1e-8 || geometry.SecondLength < 1e-8)
            {
                EE_CORE_ERROR("Two-bone IK requires nonzero segment lengths.")
                return false;
            }

            return true;
        }

        /**
         * @brief Clamp the target to the chain's reach and select a stable bend plane.
         * @param geometry Current joint positions and valid segment lengths.
         * @param constraint Target and pole in skeleton space.
         * @return Desired segment vectors with the original chain lengths.
         */
        SDesiredSegments FindReachableSegments(
            const SChainGeometry& geometry,
            const STwoBoneIKConstraint& constraint
        )
        {
            const auto firstLength = geometry.FirstLength;
            const auto secondLength = geometry.SecondLength;

            const auto target = glm::dvec3(constraint.TargetPosition) - geometry.Start;
            const auto targetDistance = glm::length(target);

            const auto currentDirection = geometry.Tip - geometry.Start;
            const auto currentDistance = glm::length(currentDirection);

            const auto forward = targetDistance > 1e-12
                ? target / targetDistance
                : currentDistance > 1e-12
                    ? currentDirection / currentDistance
                    : (geometry.Joint - geometry.Start) / firstLength;

            auto bend = glm::dvec3(constraint.PolePosition) - geometry.Start;
            bend -= forward * glm::dot(bend, forward);

            if (glm::length(bend) < 1e-8)
            {
                bend = geometry.Joint - geometry.Start;
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
            return { desiredFirst, forward * distance - desiredFirst };
        }

        /**
         * @brief Calculate parent-relative rotations that align both chain segments.
         * @param chain Bone indices and accumulated parent orientation.
         * @param geometry Current joint positions and segment lengths.
         * @param desired Reachable segment vectors selected for the target and pole.
         * @param pose Current candidate local rotations.
         * @return Solved root and middle rotations before blending.
         */
        SLocalRotations SolveRotations(
            const SIKChain& chain,
            const SChainGeometry& geometry,
            const SDesiredSegments& desired,
            const SLocalPose& pose
        )
        {
            const auto& transforms = pose.LocalTransforms;

            const auto rootDelta = RotationBetween(
                (geometry.Joint - geometry.Start) / geometry.FirstLength,
                glm::normalize(desired.First)
            );
            const auto oldRoot = glm::normalize(
                chain.ParentRotation * glm::dquat(transforms[chain.Root].Rotation)
            );
            const auto newRoot = glm::normalize(rootDelta * oldRoot);

            const auto oldMiddle =
                glm::normalize(oldRoot * glm::dquat(transforms[chain.Middle].Rotation));
            const auto middleDelta = RotationBetween(
                rootDelta * ((geometry.Tip - geometry.Joint) / geometry.SecondLength),
                glm::normalize(desired.Second)
            );
            const auto newMiddle = glm::normalize(middleDelta * rootDelta * oldMiddle);

            return {
                glm::quat(glm::normalize(glm::conjugate(chain.ParentRotation) * newRoot)),
                glm::quat(glm::normalize(glm::conjugate(newRoot) * newMiddle))
            };
        }

        /**
         * @brief Blend solved rotations into the candidate without changing other channels.
         * @param constraint Weight applied to both local rotations.
         * @param chain Bone indices identifying the two writable rotations.
         * @param rotations Solved parent-relative rotations.
         * @param pose Candidate local pose modified in place.
         */
        void BlendLocalRotations(
            const STwoBoneIKConstraint& constraint,
            const SIKChain& chain,
            const SLocalRotations& rotations,
            SLocalPose& pose
        )
        {
            auto& transforms = pose.LocalTransforms;

            const auto root = chain.Root;
            const auto middle = chain.Middle;

            const auto& rootLocal = rotations.Root;
            const auto& middleLocal = rotations.Middle;

            const auto rootRotation = glm::slerp(
                transforms[root].Rotation,
                rootLocal,
                constraint.Weight
            );
            transforms[root].Rotation = glm::normalize(rootRotation);

            const auto middleRotation = glm::slerp(
                transforms[middle].Rotation,
                middleLocal,
                constraint.Weight
            );
            transforms[middle].Rotation = glm::normalize(middleRotation);
        }
        /**
         * @brief Blend the end bone toward an optional skeleton-space orientation.
         * @param constraint Optional target orientation and shared rotation weight.
         * @param chain Bone indices and accumulated parent orientation.
         * @param pose Candidate pose after blending the two parent rotations.
         */
        void ApplyTargetRotation(
            const STwoBoneIKConstraint& constraint,
            const SIKChain& chain,
            SLocalPose& pose
        )
        {
            if (!constraint.TargetRotation)
                return;

            auto& transforms = pose.LocalTransforms;

            auto blendedRoot = chain.ParentRotation *
                glm::dquat(transforms[chain.Root].Rotation);
            blendedRoot = glm::normalize(blendedRoot);

            auto blendedMiddle = blendedRoot *
                glm::dquat(transforms[chain.Middle].Rotation);
            blendedMiddle = glm::normalize(blendedMiddle);

            auto currentEnd = blendedMiddle *
                glm::dquat(transforms[chain.End].Rotation);
            currentEnd = glm::normalize(currentEnd);

            const auto targetEnd = glm::normalize(
                glm::slerp(
                    currentEnd,
                    glm::normalize(glm::dquat(*constraint.TargetRotation)),
                    double(constraint.Weight)
                )
            );

            transforms[chain.End].Rotation =
                glm::quat(glm::normalize(glm::conjugate(blendedMiddle) * targetEnd));
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
            if (!ValidateConstraint(constraint, *m_Skeleton))
                return false;

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

        for (const auto& constraint : m_Constraints)
        {
            if (constraint.Weight == 0) continue;

            SIKChain chain;
            SChainGeometry geometry;

            if (!PrepareChain(context, constraint, pose, chain) ||
                !MeasureChain(chain, model, geometry))
                return false;

            const auto desired = FindReachableSegments(geometry, constraint);
            const auto rotations = SolveRotations(chain, geometry, desired, pose);

            BlendLocalRotations(constraint, chain, rotations, pose);
            ApplyTargetRotation(constraint, chain, pose);

            if (!skeleton.BuildModelPose(pose, model))
                return false;
        }

        return true;
    }

    TwoBoneIK::TwoBoneIK(Ref<const Skeleton> skeleton)
      : m_Skeleton(std::move(skeleton)) {}
}
