#pragma once

#include <Engine/Animation/AnimationNode.h>

namespace Elixir
{
    /** Associates one animation node with a named state. */
    struct ELIXIR_API SAnimationState
    {
        std::string Name;
        Ref<const AnimationNode> Node;

        /** Numeric input bindings in the source's declared order. */
        std::vector<std::string> InputParameters;

        bool Looping = true;

        /** @brief Check this state's source and intrinsic input configuration. */
        bool IsValid() const;
    };

    /** Defines a transition; all conditions must match, and an empty list always matches. */
    struct ELIXIR_API SAnimationTransition
    {
        std::string From;
        std::string To;
        std::vector<SAnimationCondition> Conditions;

        /** Finite nonnegative cross-fade duration in seconds. */
        float FadeDuration = 0.2f;

        /** Require a nonlooping source to reach its end and finish its incoming fade. */
        bool WaitForCompletion = false;

        /**
         * @brief Validate endpoints, timing, and intrinsic conditions against the graph.
         * @param states State definitions available to the transition.
         */
        bool IsValid(std::span<const SAnimationState> states) const;
    };

    /** Defines one state machine whose transitions read the animation's parameters. */
    struct ELIXIR_API SStateMachineCreateInfo
    {
        std::string InitialState;
        std::vector<SAnimationState> States;

        /** Declaration order sets priority when several outgoing transitions match. */
        std::vector<SAnimationTransition> Transitions;

        /** @brief Check graph structure and compatible child sources without parameters. */
        bool IsValid() const;

        /** @brief Check whether no machine definition was supplied. */
        bool IsEmpty() const
        {
            return InitialState.empty() && States.empty() && Transitions.empty();
        }
    };

    /** Immutable state-machine resource whose states select connected animation nodes. */
    class ELIXIR_API StateMachine final
    {
    public:
        /**
         * @brief Validate a state graph and compile its named destinations.
         * @param info States, conditions, and transitions to retain.
         * @return Null for an invalid graph or incompatible source skeletons.
         */
        static Ref<StateMachine> Create(SStateMachineCreateInfo info);

        /** @brief Get the common skeleton required by every state source. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /**
         * @brief Validate this graph's conditions and child input bindings.
         * @param parameters Character parameter layout.
         * @param bindings Must be empty; child sources bind their own inputs.
         */
        bool CanBind(
            const AnimationParameters& parameters,
            std::span<const std::string> bindings
        ) const;

        /** @brief Inspect the immutable authored graph. */
        const SStateMachineCreateInfo& GetCreateInfo() const { return m_CreateInfo; }

        /** @brief Get the compiled initial state index. */
        size_t GetInitialState() const { return m_InitialState; }

        /**
         * @brief Get a transition's compiled destination.
         * @param transition Index in the authored transition list.
         */
        size_t GetTarget(const size_t transition) const { return m_Targets[transition]; }

        /**
         * @brief Get outgoing transition indices in declaration order.
         * @param state Compiled source state index.
         */
        std::span<const size_t> GetTransitions(const size_t state) const
        {
            return m_Outgoing[state];
        }

    private:
        /**
         * @brief Retain a validated immutable state graph.
         * @param info Validated states and transitions.
         * @param skeleton Common skeleton retained by state sources.
         * @param initial Compiled initial state index.
         * @param targets Compiled transition destinations.
         * @param outgoing Ordered outgoing transitions per state.
         */
        StateMachine(
            SStateMachineCreateInfo info,
            Ref<const Skeleton> skeleton,
            size_t initial,
            std::vector<size_t> targets,
            std::vector<std::vector<size_t>> outgoing
        );

        SStateMachineCreateInfo m_CreateInfo;

        Ref<const Skeleton> m_Skeleton;

        size_t m_InitialState;
        std::vector<size_t> m_Targets;
        std::vector<std::vector<size_t>> m_Outgoing;
    };
}
