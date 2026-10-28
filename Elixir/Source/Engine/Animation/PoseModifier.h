#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationCurve.h>

namespace Elixir
{
    /** Supplies the fixed rig and protected movement frame for a pose correction. */
    struct SPoseModifierContext
    {
        /** Skeleton defining the candidate pose's bone order and hierarchy. */
        const Skeleton& Rig;

        /** Extracted root-motion bone whose model transform must remain unchanged, if any. */
        std::optional<uint32_t> RootMotionBone;

        /** Borrowed scalar channels for this candidate frame; null means no curves. */
        const AnimationCurveValues* Curves = nullptr;
    };

    /** Corrects candidate poses independently of animation playback and gameplay controls. */
    class ELIXIR_API PoseModifier
    {
    public:
        virtual ~PoseModifier();

        /**
         * @brief Correct a candidate local pose without advancing or mutating external state.
         *
         * Animation rebuilds model matrices and validates the result after each modifier.
         * Preserve the extracted root-motion bone's model transform when one is provided.
         * Read and configure modifiers on the same thread that evaluates their poses.
         *
         * @param context Rig and protected movement frame for this evaluation.
         * @param pose Candidate local transforms; changes are discarded if evaluation fails.
         * @return True on success; false rejects the enclosing animation frame.
         */
        virtual bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const = 0;
    };
}
