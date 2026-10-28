#pragma once

#include <Engine/Animation/PoseModifier.h>

namespace Elixir
{
    /** Adjusts two connected segments toward a skeleton-space target without stretching. */
    struct STwoBoneIKConstraint
    {
        /** End bone; its parent and grandparent form the two directly connected segments. */
        uint32_t EndBoneIndex = 0;

        /** Desired end position in the same model space as the supplied skeleton pose. */
        glm::vec3 TargetPosition{};

        /** Skeleton-space point toward which the middle joint bends. */
        glm::vec3 PolePosition{};

        /** Rotation blend in [0, 1]; zero preserves the animated pose exactly. */
        float Weight = 1.0f;

        /** Optional unit quaternion in skeleton space; absent preserves end local rotation. */
        std::optional<glm::quat> TargetRotation;
    };

    /** Owns gameplay-configured limb constraints and solves them on candidate poses. */
    class ELIXIR_API TwoBoneIK final : public PoseModifier
    {
    public:
        /**
         * @brief Create a solver bound to a fixed skeleton hierarchy.
         * @param skeleton Rig used to validate constraint indices.
         * @return A solver with no constraints, or null for an absent skeleton.
         */
        static Ref<TwoBoneIK> Create(Ref<const Skeleton> skeleton);

        /**
         * @brief Replace ordered limb targets without changing animation playback.
         *
         * Targets and poles use skeleton space. Each end bone's parent and grandparent
         * form its chain. Active chains require nonzero lengths and uniform positive scales.
         * Parent rotations solve position; an optional target also controls end orientation.
         * The target blends from the end orientation produced by the position correction.
         * Without a target rotation, preserve the end bone's local rotation.
         * Unreachable targets clamp without stretching.
         * Later constraints use earlier results. Weight blends rotations, not end positions.
         * Configure and evaluate this solver on the same thread.
         *
         * @param constraints Copied ordered list; empty disables corrections.
         * @return False for invalid settings or bones; retain the previous constraints.
         */
        bool SetConstraints(std::span<const STwoBoneIKConstraint> constraints);

        /**
         * @brief Solve limb rotations after animation layers and root-motion locking.
         * @param context Compatible rig and optional protected root-motion bone.
         * @param pose Candidate pose; discard it on failure because changes may be partial.
         * @return False for an incompatible rig, invalid pose, or unsupported active chain.
         */
        bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const override;

        /** @brief Inspect configured constraints until the next SetConstraints call. */
        std::span<const STwoBoneIKConstraint> GetConstraints() const { return m_Constraints; }

    private:
        /**
         * @brief Retain the rig used to validate constraint indices.
         * @param skeleton Non-null fixed skeleton.
         */
        explicit TwoBoneIK(Ref<const Skeleton> skeleton);

        Ref<const Skeleton> m_Skeleton;
        std::vector<STwoBoneIKConstraint> m_Constraints;
    };
}
