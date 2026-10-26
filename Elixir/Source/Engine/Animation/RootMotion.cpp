#include "epch.h"
#include "RootMotion.h"

#include <Engine/Animation/AnimationTransition.h>
#include <Engine/Animation/AnimationMontagePlayback.h>
#include <Engine/Animation/AnimationEvaluation.h>
#include <Engine/Animation/AnimationLayer.h>

namespace Elixir
{
    namespace
    {
        /**
         * @brief Extract playback movement with its transition and frame offset.
         * @param context Layer sources and requested root interval.
         * @param output Receives playback movement on success.
         */
        bool ExtractPlayback(
            const SRootMotionEvaluationContext& context,
            SRootMotionDelta& output
        )
        {
            if (!context.Playing)
            {
                output = {};
                return true;
            }

            return context.Transition.ExtractRootMotion(
                context.Playback,
                context.Interval,
                output
            );
        }

        /**
         * @brief Combine a visited montage interval with the continuing layer motion.
         * @param context Layer sources and clipped frame interval.
         * @param interval Clipped montage cursor and duration.
         * @param output Receives the blended movement on success.
         */
        bool ExtractMontageInterval(
            const SRootMotionEvaluationContext& context,
            const SAnimationMontageInterval& interval,
            SRootMotionDelta& output
        )
        {
            SRootMotionDelta source;
            if (!ExtractPlayback(context, source))
                return false;

            const auto& sequence = context.Montage.GetAsset()->GetSequence();
            const auto duration = sequence->GetDuration();

            SRootMotionDelta action;
            const auto extracted = sequence->ExtractRootMotion(
                interval.Start.Time / duration,
                (interval.Start.Time + interval.Duration) / duration,
                false,
                context.Interval.Reference,
                action,
                context.Interval.BoneIndex
            );

            if (!extracted) return false;

            const auto weight = context.Montage.GetAverageWeight(interval);
            output = SRootMotionDelta::Blend(source, action, weight);

            return output.IsValid();
        }

        /**
         * @brief Extract only the requested part of an evaluated montage frame.
         * @param context Continuing sources, montage intervals, and requested subinterval.
         * @param output Receives the complete movement on success.
         */
        bool ExtractMontage(
            const SRootMotionEvaluationContext& context,
            SRootMotionDelta& output
        )
        {
            if (!context.Montage.IsPlaying() || context.Interval.DeltaSeconds == 0)
            {
                output = {};
                return true;
            }

            const auto start = double(context.Interval.OffsetSeconds);
            const auto end = start + context.Interval.DeltaSeconds;
            SRootMotionDelta result;

            for (const auto& visited : context.MontageEvaluation.Intervals)
            {
                const auto first = std::max(start, visited.TimeOffset);
                const auto last = std::min(end, visited.TimeOffset + visited.Duration);
                if (last <= first) continue;

                const auto skipped = first - visited.TimeOffset;
                auto clipped = visited;

                clipped.Start.Time += skipped;
                clipped.Start.Elapsed += skipped;
                clipped.Duration = last - first;
                clipped.TimeOffset = first;

                const SRootMotionIntervalContext interval{
                    context.Interval.Reference,
                    context.Interval.BoneIndex,
                    float(clipped.Duration),
                    float(first)
                };

                const SRootMotionEvaluationContext slice{
                    context.Playback,
                    context.Transition,
                    context.Montage,
                    context.MontageEvaluation,
                    interval,
                    context.Playing
                };

                SRootMotionDelta movement;
                if (!ExtractMontageInterval(slice, clipped, movement))
                    return false;

                result = result * movement;
            }

            const auto remainingStart = std::max(
                start,
                context.MontageEvaluation.ActiveDuration
            );

            if (remainingStart < end)
            {
                const SRootMotionIntervalContext interval{
                    context.Interval.Reference,
                    context.Interval.BoneIndex,
                    float(end - remainingStart),
                    float(remainingStart)
                };

                const SRootMotionEvaluationContext remaining{
                    context.Playback,
                    context.Transition,
                    context.Montage,
                    context.MontageEvaluation,
                    interval,
                    context.Playing
                };

                SRootMotionDelta movement;
                if (!ExtractPlayback(remaining, movement))
                    return false;

                result = result * movement;
            }

            if (!result.IsValid())
            {
                EE_CORE_ERROR("Montage produced an invalid root-motion delta.")
                return false;
            }

            output = result;
            return true;
        }

        /**
         * @brief Extract the selected state with its outgoing transition.
         * @param context Selected source and requested root interval.
         * @param selected Borrowed selected playback and transition.
         * @param output Receives selected movement on success.
         */
        bool ExtractSelected(
            const SAnimationMotionContext& context,
            const SAnimationSelectedSources& selected,
            SRootMotionDelta& output
        )
        {
            if (!selected.Playback || !selected.Transition)
            {
                EE_CORE_ERROR("Animation movement requires a selected child and transition.")
                return false;
            }

            return selected.Transition->ExtractRootMotion(
                *selected.Playback,
                context.Interval,
                output
            );
        }

        /**
         * @brief Extract the base child layer while preserving its transitions and montage.
         * @param context Composed source and requested root interval.
         * @param layers Borrowed child layers and candidate evaluations.
         * @param output Receives base-layer movement on success.
         */
        bool ExtractLayers(
            const SAnimationMotionContext& context,
            const SAnimationLayerSources& layers,
            SRootMotionDelta& output
        )
        {
            if (layers.Layers.empty())
            {
                output = {};
                return true;
            }

            if (!layers.Layers.front())
                return false;

            const auto& base = *layers.Layers.front();

            if (!layers.Prepared)
                return base.ExtractRootMotion(context.Interval, output);

            if (layers.Evaluations.size() != layers.Layers.size())
                return false;

            return base.ExtractRootMotion(
                context.Interval,
                output,
                layers.Evaluations.front()
            );
        }

        /**
         * @brief Blend root movement from resolved sequence contributions.
         * @param context Sequence contributions, phase, and requested root interval.
         * @param output Receives blended sequence movement on success.
         */
        bool ExtractSequences(const SAnimationMotionContext& context, SRootMotionDelta& output)
        {
            const auto& state = context.State;

            if (state.Duration == 0 || context.Interval.DeltaSeconds == 0)
            {
                output = {};
                return true;
            }

            const auto& samples = std::get<SSequenceSources>(state.Sources);
            if (samples.Contributions.empty())
            {
                output = {};
                return true;
            }

            const auto offsetSeconds = double(context.Interval.OffsetSeconds);
            const auto deltaSeconds = double(context.Interval.DeltaSeconds);

            const auto from = context.Phase + offsetSeconds / state.Duration;
            const auto to = from + deltaSeconds / state.Duration;

            const auto start = context.Looping
                ? from
                : std::clamp(from, 0.0, 1.0);
            const auto end = context.Looping
                ? to
                : std::clamp(to, 0.0, 1.0);

            SRootMotionDelta result;

            float total = 0;
            for (const auto& sample : samples.Contributions)
            {
                if (sample.Weight == 0) continue;

                const auto* sync = samples.SynchronizationSequence;

                const auto first = sync
                    ? sample.Sequence->GetSynchronizedPhase(start, *sync)
                    : start;

                const auto last = sync
                    ? sample.Sequence->GetSynchronizedPhase(end, *sync)
                    : end;

                SRootMotionDelta delta;
                const auto extracted = sample.Sequence->ExtractRootMotion(
                    first,
                    last,
                    true,
                    context.Interval.Reference,
                    delta,
                    context.Interval.BoneIndex
                );

                if (!extracted) return false;

                const auto weight = sample.Weight / (total + sample.Weight);
                result = total == 0
                    ? delta
                    : SRootMotionDelta::Blend(result, delta, weight);

                total += sample.Weight;
            }

            if (total <= 0 || !result.IsValid())
                return false;

            output = result;
            return true;
        }
    }

    /* SRootMotionIntervalContext */

    bool SRootMotionIntervalContext::IsValid() const
    {
        return Reference.IsValid() &&
            std::isfinite(DeltaSeconds) && DeltaSeconds >= 0 &&
            std::isfinite(OffsetSeconds) && OffsetSeconds >= 0 &&
            std::isfinite(OffsetSeconds + DeltaSeconds);
    }

    /* SRootMotionEvaluationContext */

    bool SRootMotionEvaluationContext::IsValid() const
    {
        if (!Interval.IsValid()) return false;
        if (!Montage.GetAsset()) return true;

        const auto activeDuration = MontageEvaluation.ActiveDuration;
        if (!std::isfinite(activeDuration) || activeDuration < 0)
            return false;

        constexpr double intervalTolerance = 1e-6;
        const auto duration = Montage.GetAsset()->GetSequence()->GetDuration();
        double end = 0;

        for (const auto& interval : MontageEvaluation.Intervals)
        {
            if (!std::isfinite(interval.Start.Time) ||
                interval.Start.Time < 0 ||
                !std::isfinite(interval.Start.Elapsed) ||
                interval.Start.Elapsed < 0 ||
                !std::isfinite(interval.Duration) ||
                interval.Duration <= 0 ||
                !std::isfinite(interval.TimeOffset) ||
                std::abs(interval.TimeOffset - end) > intervalTolerance ||
                interval.Start.Time + interval.Duration > duration + intervalTolerance)
                return false;

            end = interval.TimeOffset + interval.Duration;
        }

        return std::isfinite(end) && std::abs(end - activeDuration) <= intervalTolerance;
    }

    /* SAnimationMotionContext */

    bool SAnimationMotionContext::IsValid() const
    {
        return Interval.IsValid() && std::isfinite(Phase) &&
            std::isfinite(State.Duration) && State.Duration >= 0;
    }

    /* RootMotion */

    bool RootMotion::Extract(
        const SRootMotionEvaluationContext& context,
        SRootMotionDelta& output
    )
    {
        if (!context.IsValid())
        {
            EE_CORE_ERROR("Root motion requires valid layer timing and montage intervals.")
            return false;
        }

        return context.Montage.GetAsset()
            ? ExtractMontage(context, output)
            : ExtractPlayback(context, output);
    }

    bool RootMotion::Extract(const SAnimationMotionContext& context, SRootMotionDelta& output)
    {
        if (!context.IsValid())
        {
            EE_CORE_ERROR("Root motion requires a valid reference, phase, and interval.")
            return false;
        }

        const auto& sources = context.State.Sources;

        if (const auto* selected = std::get_if<SAnimationSelectedSources>(&sources))
            return ExtractSelected(context, *selected, output);

        if (const auto* layers = std::get_if<SAnimationLayerSources>(&sources))
            return ExtractLayers(context, *layers, output);

        return ExtractSequences(context, output);
    }
}
