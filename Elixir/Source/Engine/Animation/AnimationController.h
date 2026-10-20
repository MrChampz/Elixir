#pragma once

#include <Engine/Animation/BlendSpace.h>

namespace Elixir
{
    /** Stores a numeric or boolean animation parameter. */
    using TAnimationParameterValue = std::variant<float, bool>;

    /** Selects how a transition compares a parameter with its condition value. */
    enum class EAnimationComparison : uint8_t
    {
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual
    };

    /** Defines a parameter and its initial value for each controller instance. */
    struct SAnimationParameter
    {
        std::string Name;
        TAnimationParameterValue DefaultValue = 0.0f;
    };

    /** Compares one parameter with a value of the same type. */
    struct SAnimationCondition
    {
        std::string Parameter;
        EAnimationComparison Comparison = EAnimationComparison::Equal;
        TAnimationParameterValue Value = 0.0f;
    };

    /** Associates exactly one sequence or blend space with a named state. */
    struct SAnimationState
    {
        std::string Name;
        Ref<const AnimationSequence> Sequence;

        Ref<const BlendSpace> BlendSpace;

        /** Numeric parameter names in axis order; count must match the blend space. */
        std::vector<std::string> BlendParameters;

        bool Looping = true;
    };

    /** Defines a transition; all conditions must match, and an empty list always matches. */
    struct SAnimationTransition
    {
        std::string From;
        std::string To;
        std::vector<SAnimationCondition> Conditions;

        /** Finite nonnegative cross-fade duration in seconds. */
        float FadeDuration = 0.2f;

        /** Require a nonlooping source to reach its end and finish its incoming fade. */
        bool WaitForCompletion = false;
    };

    /** Defines an immutable state machine shared by animated characters. */
    struct SAnimationControllerCreateInfo
    {
        std::string Name;
        std::string InitialState;
        std::vector<SAnimationParameter> Parameters;
        std::vector<SAnimationState> States;

        /** Declaration order sets priority when several outgoing transitions match. */
        std::vector<SAnimationTransition> Transitions;
    };

    /**
     * Stores validated animation states and transitions without per-character playback data.
     */
    class ELIXIR_API AnimationController final
    {
        friend class AnimationControllerInstance;

      public:
        AnimationController(const AnimationController&) = delete;
        AnimationController& operator=(const AnimationController&) = delete;

        /**
         * @brief Create a shared state machine, or return null for an invalid definition.
         * @param info States, typed parameters, transitions, and initial state.
         */
        static Ref<AnimationController> Create(SAnimationControllerCreateInfo info);

        /** @brief Get the controller asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the skeleton compatible with every animation in this controller. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get the immutable state definitions. */
        const std::vector<SAnimationState>& GetStates() const { return m_States; }

        /** @brief Get parameter names, types, and default values. */
        const std::vector<SAnimationParameter>& GetParameters() const { return m_Parameters; }

      private:
        struct SCondition
        {
            size_t Parameter;
            EAnimationComparison Comparison;
            TAnimationParameterValue Value;
        };

        struct STransition
        {
            size_t Target;
            std::vector<SCondition> Conditions;
            float FadeDuration;
            bool WaitForCompletion;
        };

        /**
         * @brief Store the validated definition with resolved state and parameter indices.
         * @param info Validated asset definition.
         * @param transitions Outgoing transitions for each state, in priority order.
         * @param initialState Index of the initial state.
         * @param blendParameters Resolved axis parameter indices for each state.
         */
        AnimationController(
            SAnimationControllerCreateInfo info,
            std::vector<std::vector<STransition>> transitions,
            size_t initialState,
            std::vector<std::vector<size_t>> blendParameters
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        std::vector<SAnimationParameter> m_Parameters;
        std::vector<SAnimationState> m_States;
        std::vector<std::vector<STransition>> m_Transitions;
        size_t m_InitialState;
        std::vector<std::vector<size_t>> m_BlendParameters;
    };
}
