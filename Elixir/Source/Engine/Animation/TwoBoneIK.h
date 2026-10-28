#pragma once

#include <Engine/Animation/PoseModifier.h>

namespace Elixir
{
    /** Selects a bone index or socket name in the solver's fixed skeleton. */
    using TIKTargetReference = std::variant<uint32_t, std::string>;

    /** Adjusts two connected segments toward a skeleton-space target without stretching. */
    struct STwoBoneIKConstraint
    {
        /** End bone; its parent and grandparent form the two directly connected segments. */
        uint32_t EndBoneIndex = 0;

        /** End position in skeleton space, or a local offset when TargetReference is set. */
        glm::vec3 TargetPosition{};

        /** Skeleton-space point toward which the middle joint bends. */
        glm::vec3 PolePosition{};

        /** Rotation blend in [0, 1]; zero preserves the animated pose exactly. */
        float Weight = 1.0f;

        /**
         * Optional unit rotation in skeleton space or the selected reference frame.
         * Absent preserves the end bone's local rotation.
         */
        std::optional<glm::quat> TargetRotation;

        /**
         * Optional bone index or socket name defining the target frame. Absent uses
         * skeleton space. Position and rotation become offsets in this frame; the pole
         * remains in skeleton space. Read from the current pose at this constraint's turn.
         */
        std::optional<TIKTargetReference> TargetReference;
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
         * Targets use skeleton space unless a bone or socket reference is selected.
         * References use the current pose, including earlier constraints. A reference must
         * remain outside the subtree of every active chain at or after its follower.
         * Rotation references require uniform positive scales through ancestors and sockets.
         * Poles always use skeleton space. Each end bone's parent and grandparent
         * form its chain. Active chains require nonzero lengths and uniform positive scales.
         * Parent rotations solve position; an optional target also controls end orientation.
         * The target blends from the end orientation produced by the position correction.
         * Without a target rotation, preserve the end bone's local rotation.
         * Unreachable targets clamp without stretching.
         * Later constraints use earlier results. Weight blends rotations, not end positions.
         * Configure and evaluate this solver on the same thread.
         *
         * @param constraints Copied ordered list; empty disables corrections.
         * @return False for invalid settings, bones, or references; retain previous constraints.
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
