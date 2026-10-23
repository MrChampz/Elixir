#include "epch.h"
#include "AnimationLayer.h"

namespace Elixir
{
    void AnimationLayer::CancelNotifyStates()
    {
        for (const auto& state : m_ActiveNotifyStates)
            m_PendingNotifyStateEnds.push_back({
                .Sequence = state.Sequence,
                .NotifyStateIndex = state.Index,
                .ActivationId = state.ActivationId,
                .Type = EAnimationNotifyStateEventType::End,
                .LayerName = m_Name,
                .Cancelled = true
            });
        m_ActiveNotifyStates.clear();
    }

    void AnimationLayer::EndNotifyStates(
        SNotifyStateEvaluation& output, const double offset, const bool cancelled
    ) const
    {
        for (const auto& state : output.Active)
            output.Events.push_back({
                .Sequence = state.Sequence,
                .NotifyStateIndex = state.Index,
                .ActivationId = state.ActivationId,
                .Type = EAnimationNotifyStateEventType::End,
                .TimeOffset = offset,
                .LayerName = m_Name,
                .Cancelled = cancelled
            });
        output.Active.clear();
    }

    bool AnimationLayer::EvaluatePlaybackNotifyStates(
        const SPlayback& playback, const double dt, const double offset,
        SNotifyStateEvaluation& output
    ) const
    {
        auto sequence = playback.Sequence;
        const AnimationSequence* reference = nullptr;
        if (playback.BlendSpace)
        {
            const auto* source = playback.BlendSpace->GetNotifySource(playback.Values);
            if (!source)
            {
                EE_CORE_ERROR("Notify states require a valid blend-space source.")
                return false;
            }
            sequence = Ref<const AnimationSequence>(playback.BlendSpace, source);
            reference = playback.BlendSpace->GetSynchronizationSequence().get();
        }
        if (!sequence || std::ranges::any_of(output.Active, [&](const auto& state)
                { return state.Sequence != sequence; }))
            EndNotifyStates(output, offset, true);

        const auto duration = GetDuration(playback);
        if (!sequence || dt == 0 || duration == 0)
            return true;

        const auto phase = playback.BlendSpace ? playback.Time : playback.Time / duration;
        const auto end = phase + dt / duration;
        std::vector<SAnimationNotifyStateInterval> intervals;
        if (!sequence->CollectNotifyStateIntervals(
                phase, end, playback.Looping, intervals, reference))
            return false;

        // A changed source or discontinuous timeline cannot retain an unrelated window.
        for (auto state = output.Active.begin(); state != output.Active.end();)
        {
            const auto retained = std::ranges::any_of(intervals, [&](const auto& interval)
            {
                return interval.NotifyStateIndex == state->Index &&
                    interval.StartPhaseOffset == 0;
            });
            if (retained)
            {
                ++state;
                continue;
            }
            output.Events.push_back({state->Sequence, state->Index, state->ActivationId,
                EAnimationNotifyStateEventType::End, offset, 0, m_Name, true});
            state = output.Active.erase(state);
        }

        for (const auto& interval : intervals)
        {
            auto active = std::ranges::find(output.Active, interval.NotifyStateIndex,
                &SActiveNotifyState::Index);
            if (active == output.Active.end())
            {
                if (output.NextActivationId == std::numeric_limits<uint64_t>::max())
                {
                    EE_CORE_ERROR("Animation notify state activation identities are exhausted.")
                    return false;
                }
                output.Active.push_back({sequence, interval.NotifyStateIndex,
                    output.NextActivationId++});
                active = std::prev(output.Active.end());
                output.Events.push_back({sequence, active->Index, active->ActivationId,
                    EAnimationNotifyStateEventType::Begin,
                    offset + interval.StartPhaseOffset * duration, 0, m_Name});
            }
            const auto overlap = (interval.EndPhaseOffset - interval.StartPhaseOffset) * duration;
            if (overlap > 0)
                output.Events.push_back({sequence, active->Index, active->ActivationId,
                    EAnimationNotifyStateEventType::Update,
                    offset + interval.EndPhaseOffset * duration, overlap, m_Name});
            if (interval.Ends)
            {
                output.Events.push_back({sequence, active->Index, active->ActivationId,
                    EAnimationNotifyStateEventType::End,
                    offset + interval.EndPhaseOffset * duration, 0, m_Name});
                output.Active.erase(active);
            }
        }
        if (!playback.Looping && end >= 1)
            EndNotifyStates(output, offset + std::max(0.0, 1 - phase) * duration, false);
        return true;
    }

    bool AnimationLayer::EvaluateNotifyStates(
        const float dt, SLayerEvaluation& evaluation
    ) const
    {
        auto& output = evaluation.NotifyStates;
        output.Active = m_ActiveNotifyStates;
        output.Events = m_PendingNotifyStateEnds;
        output.NextActivationId = m_NextNotifyStateActivationId;

        if (m_Weight == 0 || !m_Mask->HasInfluence())
            EndNotifyStates(output, 0, true);
        else if (!m_Montage.Montage)
        {
            if (m_Playing && !EvaluatePlaybackNotifyStates(m_Playback, dt, 0, output))
                return false;
        }
        else
        {
            const auto& montage = evaluation.Montage;
            for (size_t i = 0; i < montage.Intervals.size(); ++i)
            {
                const auto& interval = montage.Intervals[i];
                if (m_Montage.StopTime < 0)
                {
                    const SPlayback action{
                        .Sequence = m_Montage.Montage->GetSequence(),
                        .Time = interval.Start.Time,
                        .Looping = false
                    };
                    if (!EvaluatePlaybackNotifyStates(
                            action, interval.Duration, interval.TimeOffset, output))
                        return false;
                }

                const auto end = interval.Start.Time + interval.Duration;
                const auto nextTime = i + 1 < montage.Intervals.size()
                    ? montage.Intervals[i + 1].Start.Time : montage.Cursor.Time;
                if (end != nextTime)
                    EndNotifyStates(output, interval.TimeOffset + interval.Duration, true);
            }
            if (montage.Cursor.Finished)
            {
                EndNotifyStates(output, montage.ActiveDuration, false);
                const auto remaining = double(dt) - montage.ActiveDuration;
                if (m_Playing && remaining > 0)
                {
                    auto playback = m_Playback;
                    playback.Time = AdvancePlayback(playback, float(montage.ActiveDuration));
                    if (!EvaluatePlaybackNotifyStates(
                            playback, remaining, montage.ActiveDuration, output))
                        return false;
                }
            }
        }

        if (output.Events.size() - m_PendingNotifyStateEnds.size() >
            AnimationSequence::MaxNotifyEventsPerUpdate)
        {
            EE_CORE_ERROR("Animation update exceeded the notify state event capacity.")
            return false;
        }
        std::ranges::stable_sort(output.Events, [](const auto& first, const auto& second)
        {
            if (first.TimeOffset != second.TimeOffset)
                return first.TimeOffset < second.TimeOffset;
            const auto priority = [](const auto type)
            {
                return type == EAnimationNotifyStateEventType::Update ? 0
                    : type == EAnimationNotifyStateEventType::End ? 1 : 2;
            };
            return priority(first.Type) < priority(second.Type);
        });
        return true;
    }
}
