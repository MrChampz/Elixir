#include "epch.h"
#include "AnimationNotifyTracker.h"
#include "AnimationPlayback.h"
#include "AnimationLayer.h"

namespace Elixir
{
    void AnimationNotifyTracker::Cancel(const std::string_view layerName)
    {
        for (const auto& state : m_Active)
        {
            m_Pending.push_back({
                .Sequence = state.Sequence,
                .NotifyIndex = state.Index,
                .ActivationId = state.ActivationId,
                .Type = EAnimationNotifyEventType::End,
                .LayerName = state.LayerName.empty()
                    ? std::string(layerName)
                    : state.LayerName,
                .Cancelled = true
            });
        }

        m_Active.clear();
    }

    void AnimationNotifyTracker::End(
        SNotifyEvaluation& output,
        const double offset,
        const bool cancelled,
        const std::string_view layerName
    )
    {
        for (const auto& state : output.Active)
        {
            output.Events.push_back({
                .Sequence = state.Sequence,
                .NotifyIndex = state.Index,
                .ActivationId = state.ActivationId,
                .Type = EAnimationNotifyEventType::End,
                .TimeOffset = offset,
                .LayerName = state.LayerName.empty()
                    ? std::string(layerName)
                    : state.LayerName,
                .Cancelled = cancelled
            });
        }

        output.Active.clear();
    }

    bool AnimationNotifyTracker::Evaluate(
        const SAnimationTraversal& traversal,
        const std::string_view layerName,
        SNotifyEvaluation& output,
        const bool allowActivation
    )
    {
        const auto offset = traversal.FrameOffset;
        const auto& sequence = traversal.Sequence;
        const auto* reference = traversal.Reference.get();

        const auto invalidSequence = std::ranges::any_of(
            output.Active,
            [&](const auto& state) { return state.Sequence != sequence; }
        );

        if (!sequence || invalidSequence || traversal.ResetActivations)
            End(output, offset, true, layerName);

        const auto duration = traversal.DurationSeconds;
        if (!sequence || traversal.StartPhase == traversal.EndPhase || duration == 0)
            return true;

        const auto phase = traversal.StartPhase;
        const auto end = traversal.EndPhase;

        std::vector<SAnimationNotifyInterval> intervals;
        const auto success = sequence->CollectNotifyIntervals(
                phase,
                end,
                traversal.Looping,
                intervals,
                reference,
                traversal.IncludeStart
        );

        if (!success) return false;

        // A changed source or discontinuous timeline cannot retain an unrelated window.
        for (auto state = output.Active.begin(); state != output.Active.end();)
        {
            const auto retained = std::ranges::any_of(
                intervals,
                [&](const auto& interval)
                {
                    return interval.NotifyIndex == state->Index &&
                        interval.StartPhaseOffset == 0;
                }
            );

            if (retained)
            {
                ++state;
                continue;
            }

            output.Events.push_back({
                state->Sequence,
                state->Index,
                state->ActivationId,
                EAnimationNotifyEventType::End,
                offset,
                0,
                std::string(layerName),
                true
            });

            state = output.Active.erase(state);
        }

        for (const auto& interval : intervals)
        {
            const auto& notify = sequence->GetNotifies()[interval.NotifyIndex];
            if (notify.Duration == 0)
            {
                output.Events.push_back({
                    .Sequence = sequence,
                    .NotifyIndex = interval.NotifyIndex,
                    .TimeOffset = offset + interval.StartPhaseOffset * duration,
                    .LayerName = std::string(layerName)
                });
                continue;
            }

            if (!allowActivation)
                continue;

            auto active = std::ranges::find(
                output.Active,
                interval.NotifyIndex,
                &SActiveNotify::Index
            );

            if (active == output.Active.end())
            {
                if (output.NextActivationId == std::numeric_limits<uint64_t>::max())
                {
                    EE_CORE_ERROR("Animation notify activation identities are exhausted.")
                    return false;
                }

                output.Active.push_back({
                    sequence,
                    interval.NotifyIndex,
                    output.NextActivationId++,
                    std::string(layerName)
                });

                active = std::prev(output.Active.end());
                output.Events.push_back({
                    sequence,
                    active->Index,
                    active->ActivationId,
                    EAnimationNotifyEventType::Begin,
                    offset + interval.StartPhaseOffset * duration,
                    0,
                    std::string(layerName)
                });
            }

            const auto overlap = (interval.EndPhaseOffset - interval.StartPhaseOffset)
                * duration;

            if (overlap > 0)
            {
                output.Events.push_back({
                    sequence,
                    active->Index,
                    active->ActivationId,
                    EAnimationNotifyEventType::Update,
                    offset + interval.EndPhaseOffset * duration,
                    overlap,
                    std::string(layerName)
                });
            }

            if (interval.Ends)
            {
                output.Events.push_back({
                    sequence,
                    active->Index,
                    active->ActivationId,
                    EAnimationNotifyEventType::End,
                    offset + interval.EndPhaseOffset * duration,
                    0,
                    std::string(layerName)
                });

                output.Active.erase(active);
            }
        }

        if (!traversal.Looping && end >= 1)
            End(
                output,
                offset + std::max(0.0, 1 - phase) * duration,
                false,
                layerName
            );

        return true;
    }

    void AnimationNotifyTracker::Begin(SNotifyEvaluation& output) const
    {
        output.Active = m_Active;
        output.Events = m_Pending;
        output.NextActivationId = m_NextActivationId;
    }

    void AnimationNotifyTracker::Apply(SNotifyEvaluation output)
    {
        m_Active = std::move(output.Active);
        m_NextActivationId = output.NextActivationId;
        m_Pending.clear();
    }

    bool AnimationNotifyTracker::EvaluatePlayback(
        const AnimationPlayback& playback,
        const double dt,
        const std::string_view layerName,
        SNotifyEvaluation& output,
        const double offset
    )
    {
        const auto state = playback.GetEvaluationState();

        if (const auto* selected = std::get_if<SAnimationSelectedSources>(&state.Sources))
        {
            if (!selected->Playback)
            {
                EE_CORE_ERROR("Animation notifications require a selected child.")
                return false;
            }

            if (selected->ResetActivations && offset == 0)
                End(output, 0, true, layerName);

            return EvaluatePlayback(*selected->Playback, dt, layerName, output, offset);
        }

        const auto* sources = std::get_if<SAnimationLayerSources>(&state.Sources);
        if (!sources)
            return Evaluate(playback.GetTraversal(dt, offset), layerName, output);

        if (!sources->Prepared) return true;

        if (sources->NotificationsConsumed)
            *sources->NotificationsConsumed = true;

        const auto end = offset + dt;

        for (const auto& evaluation : sources->Evaluations)
        {
            for (auto event : evaluation.Notifies.Events)
            {
                if (event.TimeOffset < offset || event.TimeOffset > end)
                    continue;

                if (layerName != BaseLayerName)
                    event.LayerName = event.LayerName == BaseLayerName
                        ? std::string(layerName)
                        : std::string(layerName) + "/" + event.LayerName;

                const auto matchesActivation = [&](const SActiveNotify& active)
                {
                    return active.Sequence == event.Sequence &&
                        active.Index == event.NotifyIndex &&
                        active.ActivationId == event.ActivationId &&
                        active.LayerName == event.LayerName;
                };

                const auto known = std::ranges::any_of(
                    output.Active,
                    matchesActivation
                );

                if (event.Type == EAnimationNotifyEventType::Update)
                {
                    const auto start = std::max(
                        offset,
                        event.TimeOffset - event.DeltaSeconds
                    );

                    event.DeltaSeconds = event.TimeOffset - start;
                    if (event.DeltaSeconds == 0) continue;

                    if (!known)
                    {
                        auto begin = event;
                        begin.Type = EAnimationNotifyEventType::Begin;
                        begin.TimeOffset = start;
                        begin.DeltaSeconds = 0;

                        output.Events.push_back(std::move(begin));
                    }
                }

                if (event.Type == EAnimationNotifyEventType::End && !known)
                    continue;

                if ((event.Type == EAnimationNotifyEventType::Begin ||
                        event.Type == EAnimationNotifyEventType::Update) && !known)
                    output.Active.push_back({
                        event.Sequence,
                        event.NotifyIndex,
                        event.ActivationId,
                        event.LayerName
                    });

                if (event.Type == EAnimationNotifyEventType::End)
                    std::erase_if(output.Active, matchesActivation);

                output.Events.push_back(std::move(event));
            }

            output.NextActivationId = std::max(
                output.NextActivationId,
                evaluation.Notifies.NextActivationId
            );
        }

        return true;
    }
}
