#include "epch.h"
#include "AnimationLayer.h"

namespace Elixir
{
    void AnimationLayer::CancelActiveNotifies()
    {
        for (const auto& state : m_ActiveNotifies)
        {
            m_PendingNotifyEnds.push_back({
                .Sequence = state.Sequence,
                .NotifyIndex = state.Index,
                .ActivationId = state.ActivationId,
                .Type = EAnimationNotifyEventType::End,
                .LayerName = m_Name,
                .Cancelled = true
            });
        }

        m_ActiveNotifies.clear();
    }

    void AnimationLayer::EndActiveNotifies(
        SNotifyEvaluation& output,
        const double offset,
        const bool cancelled
    ) const
    {
        for (const auto& state : output.Active)
        {
            output.Events.push_back({
                .Sequence = state.Sequence,
                .NotifyIndex = state.Index,
                .ActivationId = state.ActivationId,
                .Type = EAnimationNotifyEventType::End,
                .TimeOffset = offset,
                .LayerName = m_Name,
                .Cancelled = cancelled
            });
        }

        output.Active.clear();
    }

    bool AnimationLayer::EvaluatePlaybackNotifyEvents(
        const SPlayback& playback,
        const double dt,
        const double offset,
        SNotifyEvaluation& output,
        const bool allowActivation
    ) const
    {
        auto sequence = playback.Sequence;
        const AnimationSequence* reference = nullptr;

        if (playback.BlendSpace)
        {
            const auto* source = playback.BlendSpace->GetNotifySource(playback.Values);
            if (!source)
            {
                EE_CORE_ERROR("Notifications require a valid blend-space source.")
                return false;
            }

            sequence = Ref<const AnimationSequence>(playback.BlendSpace, source);
            reference = playback.BlendSpace->GetSynchronizationSequence().get();
        }

        const auto invalidSequence = std::ranges::any_of(
            output.Active,
            [&](const auto& state) { return state.Sequence != sequence; }
        );

        if (!sequence || invalidSequence)
            EndActiveNotifies(output, offset, true);

        const auto duration = GetDuration(playback);
        if (!sequence || dt == 0 || duration == 0)
            return true;

        const auto phase = playback.BlendSpace
            ? playback.Time
            : playback.Time / duration;

        const auto end = phase + dt / duration;

        std::vector<SAnimationNotifyInterval> intervals;
        const auto success = sequence->CollectNotifyIntervals(
                phase,
                end,
                playback.Looping,
                intervals,
                reference,
                playback.IncludeNotifyStart
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
                m_Name,
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
                    .LayerName = m_Name
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
                    output.NextActivationId++
                });

                active = std::prev(output.Active.end());
                output.Events.push_back({
                    sequence,
                    active->Index,
                    active->ActivationId,
                    EAnimationNotifyEventType::Begin,
                    offset + interval.StartPhaseOffset * duration,
                    0,
                    m_Name
                });
            }

            const auto overlap = (interval.EndPhaseOffset - interval.StartPhaseOffset) * duration;
            if (overlap > 0)
            {
                output.Events.push_back({
                    sequence,
                    active->Index,
                    active->ActivationId,
                    EAnimationNotifyEventType::Update,
                    offset + interval.EndPhaseOffset * duration,
                    overlap,
                    m_Name
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
                    m_Name
                });

                output.Active.erase(active);
            }
        }

        if (!playback.Looping && end >= 1)
            EndActiveNotifies(
                output,
                offset + std::max(0.0, 1 - phase) * duration,
                false
            );

        return true;
    }

    bool AnimationLayer::EvaluateNotifyEvents(
        const float dt,
        SLayerEvaluation& evaluation
    ) const
    {
        auto& output = evaluation.Notifies;
        output.Active = m_ActiveNotifies;
        output.Events = m_PendingNotifyEnds;
        output.NextActivationId = m_NextNotifyActivationId;

        if (m_Weight == 0 || !m_Mask->HasInfluence())
        {
            EndActiveNotifies(output, 0, true);
        }
        else if (!m_Montage.Montage)
        {
            if (m_Playing &&
                !EvaluatePlaybackNotifyEvents(m_Playback, dt, 0, output))
                return false;
        }
        else
        {
            const auto& montage = evaluation.Montage;
            for (size_t i = 0; i < montage.Intervals.size(); ++i)
            {
                const auto& interval = montage.Intervals[i];
                const SPlayback action{
                    .Sequence = m_Montage.Montage->GetSequence(),
                    .Time = interval.Start.Time,
                    .IncludeNotifyStart = interval.Start.IncludeNotifyStart,
                    .Looping = false
                };

                const auto success = EvaluatePlaybackNotifyEvents(
                    action,
                    interval.Duration,
                    interval.TimeOffset,
                    output,
                    m_Montage.StopTime < 0
                );

                if (!success) return false;

                const auto end = interval.Start.Time + interval.Duration;

                const auto nextIndex = i + 1;
                const auto nextTime = nextIndex < montage.Intervals.size()
                    ? montage.Intervals[nextIndex].Start.Time
                    : montage.Cursor.Time;

                if (end != nextTime)
                    EndActiveNotifies(
                        output,
                        interval.TimeOffset + interval.Duration,
                        true
                    );
            }

            if (montage.Cursor.Finished)
            {
                EndActiveNotifies(output, montage.ActiveDuration, false);
                const auto remaining = double(dt) - montage.ActiveDuration;

                if (m_Playing && remaining > 0)
                {
                    auto playback = m_Playback;
                    playback.Time = AdvancePlayback(playback, float(montage.ActiveDuration));
                    playback.IncludeNotifyStart = false;

                    const auto success = EvaluatePlaybackNotifyEvents(
                        playback,
                        remaining,
                        montage.ActiveDuration,
                        output
                    );
                    if (!success) return false;
                }
            }
        }

        if (output.Events.size() - m_PendingNotifyEnds.size() >
            AnimationSequence::MaxNotifyEventsPerUpdate)
        {
            EE_CORE_ERROR("Animation update exceeded the notify event capacity.")
            return false;
        }

        std::ranges::stable_sort(output.Events, [](const auto& first, const auto& second)
        {
            if (first.TimeOffset != second.TimeOffset)
                return first.TimeOffset < second.TimeOffset;

            const auto priority = [](const auto type)
            {
                return type == EAnimationNotifyEventType::Update
                    ? 0
                    : type == EAnimationNotifyEventType::End
                        ? 1
                        : 2;
            };

            return priority(first.Type) < priority(second.Type);
        });

        return true;
    }
}
