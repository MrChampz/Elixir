#pragma once

#include <Engine/Animation/AnimationLayer.h>

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

    /** Defines one state machine whose transitions read the controller's parameters. */
    struct SAnimationStateMachine
    {
        std::string InitialState;
        std::vector<SAnimationState> States;

        /** Declaration order sets priority when several outgoing transitions match. */
        std::vector<SAnimationTransition> Transitions;
    };

    /** Name of the permanent first animation layer. */
    static constexpr std::string BaseLayerName = "Base";

    /** Defines a layer's state machine and its ordered contribution to the final pose. */
    struct SAnimationControllerLayer
    {
        /** Unique layer name; the first layer must be Base. */
        std::string Name = BaseLayerName;

        /** Required for nonbase layers; Base uses the mesh's full-body mask. */
        Ref<const BoneMask> Mask;

        /** Finite contribution in [0, 1]; Base requires one. */
        float Weight = 1.0f;

        EAnimationLayerBlendMode BlendMode = EAnimationLayerBlendMode::Override;

        /** Empty selects the mesh reference pose for additive differences. */
        std::vector<SBoneTransform> AdditiveReferencePose;

        SAnimationStateMachine StateMachine;
    };

    /** Defines a character's shared parameters, state machines, and pose composition. */
    struct SAnimationControllerCreateInfo
    {
        std::string Name;
        std::vector<SAnimationParameter> Parameters;

        /** Composition order, starting with Base; every layer has its own state machine. */
        std::vector<SAnimationControllerLayer> Layers;
    };

    /**
     * Stores a character animation definition without per-character playback data.
     */
    class ELIXIR_API AnimationController final
    {
        friend class AnimationControllerInstance;

      public:
        AnimationController(const AnimationController&) = delete;
        AnimationController& operator=(const AnimationController&) = delete;

        /**
         * @brief Create a shared character controller, or return null for invalid definitions.
         * @param info Shared parameters and ordered layers with their state machines.
         */
        static Ref<AnimationController> Create(SAnimationControllerCreateInfo info);

        /** @brief Get the controller asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the skeleton compatible with every animation in this controller. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get ordered layer settings and immutable state-machine definitions. */
        const std::vector<SAnimationControllerLayer>& GetLayers() const { return m_Layers; }

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

        struct SStateMachine
        {
            size_t InitialState;
            std::vector<std::vector<STransition>> Transitions;
            std::vector<std::vector<size_t>> BlendParameters;
        };

        /**
         * @brief Validate and resolve one machine against shared controller parameters.
         * @param machine Authored states and transitions.
         * @param parameters Shared typed parameter definitions.
         * @param skeleton Common skeleton, initialized by the first valid state.
         */
        static std::optional<SStateMachine> CompileStateMachine(
            const SAnimationStateMachine& machine,
            const std::vector<SAnimationParameter>& parameters,
            Ref<const Skeleton>& skeleton
        );

        /**
         * @brief Store validated layers and their resolved state machines.
         * @param info Validated controller definition.
         * @param skeleton Common skeleton for every layer.
         * @param machines Resolved transitions and parameter bindings per layer.
         */
        AnimationController(
            SAnimationControllerCreateInfo info,
            Ref<const Skeleton> skeleton,
            std::vector<SStateMachine> machines
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        std::vector<SAnimationParameter> m_Parameters;
        std::vector<SAnimationControllerLayer> m_Layers;
        std::vector<SStateMachine> m_StateMachines;
    };
}
