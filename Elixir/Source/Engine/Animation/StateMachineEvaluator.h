#pragma once

#include <Engine/Animation/AnimationNodeEvaluator.h>
#include <Engine/Animation/StateMachine.h>
#include <Engine/Animation/AnimationTransition.h>

namespace Elixir
{
    /** Independent pose evaluation and child state for a StateMachine resource. */
    class ELIXIR_API StateMachineEvaluator final : public AnimationNodeEvaluator
    {
    public:
        /**
         * @brief Borrow a resource retained by the enclosing playback.
         * @param source Resource that outlives this evaluator.
         */
        explicit StateMachineEvaluator(const StateMachine& source);

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

        /**
         * @brief Publish child state after complete frame validation.
         * @param phase Prepared normalized position.
         */
        void Apply(double phase) override;

        /**
         * @brief Reset children after playback relocates its clock.
         * @param phase New normalized position.
         */
        bool Reset(double phase) override;

        /** @brief Describe resolved timing and contributing sources. */
        SAnimationNodeState GetState() const override;

    private:
        /**
         * @brief Prepare child clocks and candidate poses.
         * @param context Frame timing, notification policy, and sampling defaults.
         */
        double Prepare(const SAnimationEvaluationContext& context);

        /**
         * @brief Sample committed or candidate child data.
         * @param phase Requested normalized position.
         * @param referencePose Defaults for unanimated channels.
         * @param output Receives the local pose.
         */
        bool Sample(double phase, const SLocalPose& referencePose, SLocalPose& output) const;

        /**
         * @brief Create and bind one state's independent source playback.
         * @param stateIndex State index.
         * @param output Receives a validated child playback.
         */
        bool CreatePlayback(size_t stateIndex, AnimationPlayback& output) const;

        /**
         * @brief Evaluate a transition using its bound condition parameter indices.
         * @param transition Index in the immutable graph's transition list.
         */
        bool Matches(size_t transition) const;

        /** @brief Get the target playback used by the candidate frame. */
        const AnimationPlayback& Target() const;

        /** @brief Get the transition used by the candidate frame. */
        const AnimationTransition& Transition() const;

        const StateMachine* m_Source;
        Ref<AnimationParameters> m_Parameters;
        std::vector<std::vector<size_t>> m_ConditionBindings;

        size_t m_State;

        AnimationPlayback m_Playback;
        std::optional<AnimationPlayback> m_NextPlayback;

        AnimationTransition m_Transition;
        std::optional<AnimationTransition> m_NextTransition;

        SLocalPose m_VisiblePose;
        SLocalPose m_NextPose;

        size_t m_NextState = 0;
        double m_NextPhase = 0;
        double m_OutgoingPhase = 0;
        float m_NextElapsed = 0;

        bool m_Prepared = false;
    };
}
