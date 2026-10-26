#pragma once

#include <Engine/Animation/AnimationNodeEvaluator.h>
#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Independent pose evaluation and child state for a AnimationSequence resource. */
    class ELIXIR_API SequenceEvaluator final : public AnimationNodeEvaluator
    {
    public:
        /**
         * @brief Borrow a resource retained by the enclosing playback.
         * @param source Resource that outlives this evaluator.
         */
        explicit SequenceEvaluator(const AnimationSequence& source);

        /** @brief Copy independent node state. */
        Scope<AnimationNodeEvaluator> Clone() const override;

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
        const AnimationSequence* m_Source;
        SSequenceContribution m_Contribution;
    };
}
