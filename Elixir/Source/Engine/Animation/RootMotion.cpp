#include "epch.h"
#include "RootMotion.h"

#include <Engine/Animation/AnimationTransition.h>
#include <Engine/Animation/AnimationMontagePlayback.h>
#include <Engine/Animation/AnimationEvaluation.h>
#include <Engine/Animation/AnimationLayer.h>

namespace Elixir
{
    bool RootMotion::Extract(
        const SRootMotionEvaluationContext& context,
        SRootMotionDelta& output
    )
    {
        const auto extractSource = [&](
            const float seconds,
            SRootMotionDelta& movement,
            const float offset
        )
        {
            if (!context.Playing)
            {
                movement = {};
                return true;
            }

            const SRootMotionIntervalContext interval{
                .Reference = context.Interval.Reference,
                .BoneIndex = context.Interval.BoneIndex,
                .DeltaSeconds = seconds,
                .OffsetSeconds = offset
            };

            return context.Transition.ExtractRootMotion(
                context.Playback,
                interval,
                movement
            );
        };

        if (!context.Montage.GetAsset())
            return extractSource(context.Interval.DeltaSeconds, output, 0);

        if (!context.Montage.IsPlaying() || context.Interval.DeltaSeconds == 0)
        {
            output = {};
            return true;
        }

        const auto& sequence = context.Montage.GetAsset()->GetSequence();
        const auto duration = sequence->GetDuration();
        SRootMotionDelta result;

        for (const auto& interval : context.MontageEvaluation.Intervals)
        {
            SRootMotionDelta source, action;

            auto success = extractSource(
                float(interval.Duration),
                source,
                float(interval.TimeOffset)
            );

            if (!success) return false;

            success = sequence->ExtractRootMotion(
                interval.Start.Time / duration,
                (interval.Start.Time + interval.Duration) / duration,
                false,
                context.Interval.Reference,
                action,
                context.Interval.BoneIndex
            );

            if (!success) return false;

            const auto weight = context.Montage.GetAverageWeight(interval);
            result = result * SRootMotionDelta::Blend(source, action, weight);
        }

        SRootMotionDelta remaining;
        const auto activeTime = float(context.MontageEvaluation.ActiveDuration);

        const auto success = extractSource(
            context.Interval.DeltaSeconds - activeTime,
            remaining,
            activeTime
        );

        if (!success) return false;

        result = result * remaining;

        if (!result.IsValid())
        {
            EE_CORE_ERROR("Montage produced an invalid root-motion delta.")
            return false;
        }

        output = result;
        return true;
    }

    bool RootMotion::Extract(const SAnimationMotionContext& context, SRootMotionDelta& output)
    {
        const auto& state = context.State;
        const auto& sources = state.Sources;

        if (const auto* selected = std::get_if<SAnimationSelectedSources>(&sources))
        {
            if (!selected->Playback || !selected->Transition)
            {
                EE_CORE_ERROR("Animation movement requires a selected child and transition.")
                return false;
            }

            return selected->Transition->ExtractRootMotion(
                *selected->Playback,
                context.Interval,
                output
            );
        }

        if (const auto* layers = std::get_if<SAnimationLayerSources>(&sources))
        {
            if (layers->Layers.empty())
            {
                output = {};
                return true;
            }

            const auto& base = *layers->Layers.front();

            if (layers->Prepared && context.Interval.OffsetSeconds == 0)
            {
                if (layers->Evaluations.size() != layers->Layers.size())
                    return false;

                return base.ExtractRootMotion(
                    context.Interval,
                    output,
                    layers->Evaluations.front()
                );
            }

            return base.m_Playback.ExtractRootMotion(context.Interval, output);
        }

        if (state.Duration == 0 || context.Interval.DeltaSeconds == 0)
        {
            output = {};
            return true;
        }

        const auto& samples = std::get<SSequenceSources>(sources);
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
