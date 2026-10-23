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

        /** Finite nonnegative transition duration in seconds. */
        float FadeDuration = 0.2f;

        /** Require a nonlooping source to reach its end and finish its incoming fade. */
        bool WaitForCompletion = false;

        /** Selects source cross-fading or destination-only inertialization. */
        EAnimationTransitionMode Mode = EAnimationTransitionMode::CrossFade;
    };

    /** Defines one state machine whose transitions read the controller's parameters. */
    struct SAnimationStateMachine
    {
        std::string InitialState;
        std::vector<SAnimationState> States;

        /** Declaration order sets priority when several outgoing transitions match. */
        std::vector<SAnimationTransition> Transitions;

        /** @brief Check whether no machine definition was supplied. */
        bool IsEmpty() const
        {
            return InitialState.empty() && States.empty() && Transitions.empty();
        }
    };

    /** Name of the permanent first animation layer. */
    static constexpr std::string BaseLayerName = "Base";

    /** Defines a layer's animation source and ordered contribution to the final pose. */
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

        /** Direct sequence source; select exactly one sequence, blend space, or machine. */
        Ref<const AnimationSequence> Sequence;

        /** Direct blend-space source, independent of its dimension count. */
        Ref<const BlendSpace> BlendSpace;

        /** Shared numeric parameter names in axis order for a direct blend space. */
        std::vector<std::string> BlendParameters;

        /** Optional state-machine source; leave empty when using a direct source. */
        SAnimationStateMachine StateMachine;

        /** Wrap direct playback; machine states define their own looping behavior. */
        bool Looping = true;
    };

    /** Defines a character's shared parameters, state machines, and pose composition. */
    struct SAnimationControllerCreateInfo
    {
        std::string Name;
        std::vector<SAnimationParameter> Parameters;

        /** Composition order, starting with Base; every layer defines one animation source. */
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
         * @param info Shared parameters and ordered layers with their animation sources.
         */
        static Ref<AnimationController> Create(SAnimationControllerCreateInfo info);

        /** @brief Get the controller asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the skeleton compatible with every animation in this controller. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get ordered layer settings and immutable animation sources. */
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
            EAnimationTransitionMode Mode;
        };

        struct SSequenceSource
        {
            Ref<const AnimationSequence> Sequence;
            bool Looping;
        };

        struct SBlendSpaceSource
        {
            Ref<const BlendSpace> BlendSpace;
            std::vector<size_t> BlendParameters;
            bool Looping;
        };

        using TPlaybackSource = std::variant<SSequenceSource, SBlendSpaceSource>;

        struct SStateMachine
        {
            size_t InitialState;
            std::vector<TPlaybackSource> Sources;
            std::vector<std::vector<STransition>> Transitions;
        };

        using TLayerSource = std::variant<SSequenceSource, SBlendSpaceSource, SStateMachine>;

        /**
         * @brief Validate sequence or blend-space playback and resolve numeric bindings.
         * @tparam TSource Layer or state definition with playback settings.
         * @param source Authored sequence, blend space, bindings, and looping settings.
         * @param parameters Shared typed parameter definitions.
         * @param skeleton Common skeleton, initialized by the first valid source.
         */
        template<typename TSource>
        static std::optional<TPlaybackSource> CompilePlaybackSource(
            const TSource& source,
            const std::vector<SAnimationParameter>& parameters,
            Ref<const Skeleton>& skeleton
        );

        /**
         * @brief Validate one layer source and resolve its parameter bindings.
         * @param layer Direct playback or a state machine, with layer settings.
         * @param parameters Shared typed parameter definitions.
         * @param skeleton Common skeleton, initialized by the first valid source.
         */
        static std::optional<TLayerSource> CompileLayerSource(
            const SAnimationControllerLayer& layer,
            const std::vector<SAnimationParameter>& parameters,
            Ref<const Skeleton>& skeleton
        );

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
         * @brief Store validated layers and their resolved animation sources.
         * @param info Validated controller definition.
         * @param skeleton Common skeleton for every layer.
         * @param sources Resolved playback or state-machine source per layer.
         */
        AnimationController(
            SAnimationControllerCreateInfo info,
            Ref<const Skeleton> skeleton,
            std::vector<TLayerSource> sources
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        std::vector<SAnimationParameter> m_Parameters;
        std::vector<SAnimationControllerLayer> m_Layers;
        std::vector<TLayerSource> m_LayerSources;
    };
}
