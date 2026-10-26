#pragma once

#include <Engine/Animation/AnimationEvaluation.h>
#include <Engine/Animation/AnimationParameters.h>

namespace Elixir
{
    /** Evaluates one node while retaining only its independent node-specific state. */
    class ELIXIR_API AnimationNodeEvaluator
    {
    public:
        virtual ~AnimationNodeEvaluator();

        /** @brief Copy committed node state for an independent candidate command. */
        virtual Scope<AnimationNodeEvaluator> Clone() const = 0;

        /**
         * @brief Consume resolved inputs without owning parameter bindings or a clock.
         * @param values Numeric coordinates resolved by the enclosing playback.
         * @param parameters Shared values available to composed child nodes.
         */
        virtual bool Configure(
            std::span<const float> values,
            Ref<AnimationParameters> parameters
        ) = 0;

        /**
         * @brief Sample a pose or prepare a complete candidate node frame.
         * @param context Resolved inputs for this evaluation.
         * @param output Receives the pose, candidate phase, and contributing sources.
         */
        virtual bool Evaluate(
            const SAnimationEvaluationContext& context,
            SAnimationEvaluation& output
        ) = 0;

        /**
         * @brief Publish child state from a successfully validated candidate frame.
         * @param phase Prepared normalized position.
         */
        virtual void Apply(double phase) {}

        /**
         * @brief Discard candidate state after the enclosing playback changes position.
         * @param phase New normalized position selected by playback.
         */
        virtual bool Reset(double phase) { return true; }

        /** @brief Describe resolved timing, source contributions, and child controls. */
        virtual SAnimationNodeState GetState() const = 0;
    };
}
