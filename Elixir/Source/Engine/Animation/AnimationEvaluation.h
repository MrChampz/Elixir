#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/BlendSpace.h>

namespace Elixir
{
    class AnimationPlayback;
    class AnimationTransition;
    class AnimationLayer;
    struct SAnimationLayerEvaluation;

    /** One borrowed clip and its contribution to a source. */
    struct SSequenceContribution
    {
        /** Clip retained by the source that produced this contribution. */
        const AnimationSequence* Sequence = nullptr;

        /**
         * Nonnegative weight; CalculateContributions returns positive weights
         * normalized to sum to one.
         */
        float Weight = 0;
    };

    /** Borrowed sequence contributions resolved by a leaf evaluator. */
    struct SAnimationSequenceSources
    {
        /** Empty for generated poses that do not contribute sequence movement. */
        std::span<const SSequenceContribution> Contributions;

        const AnimationSequence* NotifySequence = nullptr;

        const AnimationSequence* SynchronizationSequence = nullptr;
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

        std::span<const SAnimationLayerEvaluation> Evaluations;

        bool Prepared = false;

        /** Candidate bookkeeping updated when a notification consumer reads these frames. */
        bool* NotificationsConsumed = nullptr;
    };

    /** Sources remain valid until their owning evaluator is changed or released. */
    using TAnimationEvaluationSources = std::variant<
        SAnimationSequenceSources,
        SAnimationSelectedSources,
        SAnimationLayerSources
    >;

    /** Resolved timing and source data; playback owns the actual clock. */
    struct SAnimationNodeState
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

        /** Forward the enclosing control's event policy to composed child controls. */
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
    };
}
