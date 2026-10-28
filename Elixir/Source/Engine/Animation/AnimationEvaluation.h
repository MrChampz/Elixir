#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationCurve.h>

namespace Elixir
{
    class Sequence;
    class AnimationPlayback;
    class AnimationTransition;
    class AnimationLayer;
    struct SAnimationPlayerEvaluation;

    /** One borrowed clip and its contribution to a source. */
    struct SSequenceContribution
    {
        /** Clip retained by the source that produced this contribution. */
        const Sequence* Sequence = nullptr;

        /**
         * Nonnegative weight; CalculateContributions returns positive weights
         * normalized to sum to one.
         */
        float Weight = 0;
    };

    /** Borrowed sequence contributions resolved by a leaf evaluator. */
    struct SSequenceSources
    {
        /** Empty for generated poses that do not contribute sequence movement. */
        std::span<const SSequenceContribution> Contributions;

        const Sequence* NotifySequence = nullptr;

        const Sequence* SynchronizationSequence = nullptr;
    };

    /** Child playback selected by a machine, including its candidate transition. */
    struct SAnimationSelectedSources
    {
        const AnimationPlayback* Playback = nullptr;

        const AnimationTransition* Transition = nullptr;

        /** A discontinuous source change cancels activations at the interval start. */
        bool ResetActivations = false;
    };

    /** Child controls and their borrowed candidate frames in a composition. */
    struct SAnimationLayerSources
    {
        std::span<const Scope<AnimationLayer>> Layers;

        std::span<const SAnimationPlayerEvaluation> Evaluations;

        bool Prepared = false;

        /** Candidate bookkeeping updated when a notification consumer reads these frames. */
        bool* NotificationsConsumed = nullptr;
    };

    /** Sources remain valid until their owning evaluator is changed or released. */
    using TAnimationEvaluationSources = std::variant<
        SSequenceSources,
        SAnimationSelectedSources,
        SAnimationLayerSources
    >;

    /** Resolved timing and source data; playback owns the actual clock. */
    struct ELIXIR_API SAnimationNodeState
    {
        /** Effective cycle duration in seconds. */
        float Duration = 0;

        /** Keep reacting to inputs and child state even when the duration is zero. */
        bool Continuous = false;

        /** Explicit composite completion; empty lets playback use its cyclic clock policy. */
        std::optional<bool> Finished;

        TAnimationEvaluationSources Sources;

        /** Committed child used for control queries, independently of a candidate frame. */
        const AnimationPlayback* PrimaryPlayback = nullptr;

        std::string_view StateName;

        bool Transitioning = false;

        /**
         * @brief Find a unique named layer, including qualified child paths.
         * @param name Layer name or slash-separated path.
         * @return Null if the layer is absent or its unqualified name is ambiguous.
         */
        AnimationLayer* GetLayer(std::string_view name) const;

        /** @brief Count leaf layers exposed by this state. */
        size_t GetLayerCount() const;

        /** @brief Get the active machine state's name, if exposed. */
        std::string_view GetCurrentState() const;

        /** @brief Check whether a child layer or active state is transitioning. */
        bool IsTransitioning() const;
    };

    /** Explicit inputs for sampling or preparing one node frame. */
    struct SAnimationEvaluationContext
    {
        /** Normalized sampling position already calculated by the enclosing playback. */
        double Phase = 0;

        /** Elapsed frame seconds; zero when only sampling. */
        float DeltaSeconds = 0;

        /** Prepare child clocks and decisions instead of only reading a pose. */
        bool Advance = false;

        /** Default notify publication policy for this frame's child sources. */
        bool NotifiesEnabled = true;

        const SLocalPose& ReferencePose;

        bool Looping = true;
    };

    /** Candidate pose and clock position, published only after frame validation. */
    struct SAnimationEvaluation
    {
        SLocalPose Pose;

        /** Candidate normalized position; does not change the published playback clock. */
        double Phase = 0;

        /** Borrowed sources consumed before the owning evaluator is changed or released. */
        TAnimationEvaluationSources Sources;

        /** Sampled scalar channels evaluated with the candidate pose. */
        AnimationCurveValues Curves;
    };
}
