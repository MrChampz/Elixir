#pragma once

#include <Engine/Animation/AnimationEvaluator.h>
#include <Engine/Animation/BlendSpace.h>

namespace Elixir
{
    /** Independent pose evaluation and child state for a BlendSpace resource. */
    class ELIXIR_API BlendSpaceEvaluator final : public AnimationEvaluator
    {
    public:
        /**
         * @brief Borrow a resource retained by the enclosing playback.
         * @param source Resource that outlives this evaluator.
         */
        explicit BlendSpaceEvaluator(const BlendSpace& source);

        /** @brief Copy independent node state. */
        Scope<AnimationEvaluator> Clone() const override;

        /**
         * @brief Consume numeric inputs and shared character parameters.
         * @param values Resolved coordinates in input order.
         * @param parameters Character values available to child nodes.
         */
        bool Configure(
            std::span<const float> values,
            Ref<AnimationParameters> parameters
        ) override;

        /**
         * @brief Produce a sampled pose or a candidate child frame.
         * @param context Requested sampling position and optional frame advance.
         * @param output Receives the candidate pose and contributing sources.
         */
        bool Evaluate(
            const SAnimationEvaluationContext& context,
            SAnimationEvaluation& output
        ) override;

        /** @brief Describe resolved timing and contributing sources. */
        SAnimationNodeState GetState() const override;

    private:
        /**
         * @brief Sample the resolved sequence contributions.
         * @param phase Normalized sampling position.
         * @param referencePose Defaults for unanimated channels.
         * @param output Receives the blended pose.
         * @param curves Receives scalar channels blended from the same contributions.
         */
        bool Sample(
            double phase,
            const SLocalPose& referencePose,
            SLocalPose& output,
            AnimationCurveValues& curves
        ) const;

        /**
         * @brief Resolve contributions when the coordinates change.
         * @param values Finite coordinates matching the resource dimensions.
         */
        bool SetInputs(std::span<const float> values);

        const BlendSpace* m_Source;
        std::vector<SSequenceContribution> m_Samples;
        std::vector<float> m_Values;
        float m_Duration = 0;
        const Sequence* m_NotifySource = nullptr;
    };
}
