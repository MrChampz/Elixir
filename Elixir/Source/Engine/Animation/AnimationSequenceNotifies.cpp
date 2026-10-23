#include "epch.h"
#include "AnimationSequence.h"

namespace Elixir
{
    bool AnimationSequence::CollectNotifyIntervals(
        const double startPhase,
        double endPhase,
        const bool looping,
        std::vector<SAnimationNotifyInterval>& output,
        const AnimationSequence* reference,
        const bool includeStart
    ) const
    {
        if (!std::isfinite(startPhase) ||
            !std::isfinite(endPhase) ||
            startPhase < 0 ||
            startPhase > 1 ||
            endPhase < startPhase)
        {
            EE_CORE_ERROR("Notify collection requires a finite forward phase interval.")
            return false;
        }

        if (!looping)
            endPhase = std::min(endPhase, 1.0);

        if (startPhase == endPhase || m_Notifies.empty())
        {
            output.clear();
            return true;
        }

        if (endPhase > 0x1p52)
        {
            EE_CORE_ERROR("Notify collection exceeded the supported phase range.")
            return false;
        }

        const auto synchronized = reference && CanSynchronizeWith(*reference);
        std::vector<SAnimationNotifyInterval> intervals;

        for (size_t index = 0; index < m_Notifies.size(); ++index)
        {
            const auto& notify = m_Notifies[index];
            auto start = double(notify.Time) / m_Duration;
            auto end = double(notify.Time + notify.Duration) / m_Duration;

            if (synchronized)
            {
                if (notify.Duration == 0)
                {
                    const auto marker = std::ranges::find(
                        m_SyncMarkers,
                        notify.Time,
                        &SAnimationSyncMarker::Time
                    );

                    if (marker != m_SyncMarkers.end())
                    {
                        const auto matching = std::ranges::find(
                            reference->m_SyncMarkers,
                            marker->Name,
                            &SAnimationSyncMarker::Name
                        );
                        start = double(matching->Time) / reference->GetDuration();
                    }
                    else
                    {
                        const auto time = reference->GetSynchronizedTime(float(start), *this);
                        start = double(time) / reference->GetDuration();
                    }

                    if (start == 1 && notify.Time < m_Duration)
                        start = 0;
                }
                else
                {
                    start = reference->GetSynchronizedPhase(start, *this);
                    end = reference->GetSynchronizedPhase(end, *this);

                    const auto cycle = std::floor(start);
                    start -= cycle;
                    end -= cycle;
                }
            }

            const auto instant = notify.Duration == 0;
            const auto atStart = instant && includeStart &&
                start == startPhase && notify.Time < m_Duration;
            auto firstCycle = 0.0;
            auto count = 0.0;

            if (instant)
            {
                if (looping)
                {
                    firstCycle = std::floor(startPhase - start) + 1;
                    const auto lastCycle = std::floor(endPhase - start);
                    count = std::max(0.0, lastCycle - firstCycle + 1);
                }
                else if (start > startPhase && start <= endPhase)
                {
                    count = 1;
                }
            }
            else
            {
                firstCycle = std::floor(startPhase - end) + 1;
                auto lastCycle = std::floor(endPhase - start);

                if (!looping && start + lastCycle >= 1)
                    lastCycle -= 1;

                count = std::max(0.0, lastCycle - firstCycle + 1);
            }

            const auto remaining = MaxNotifyEventsPerUpdate - intervals.size();
            if (count + double(atStart) > double(remaining))
            {
                EE_CORE_ERROR("Animation update exceeded the notify interval capacity.")
                return false;
            }

            if (atStart)
                intervals.push_back({index, 0, 0, true});

            for (size_t i = 0; i < size_t(count); ++i)
            {
                const auto begin = start + firstCycle + double(i);
                const auto finish = instant ? begin : end + firstCycle + double(i);
                intervals.push_back({
                    index,
                    std::max(startPhase, begin) - startPhase,
                    std::min(endPhase, finish) - startPhase,
                    instant || finish <= endPhase
                });
            }
        }

        std::ranges::stable_sort(intervals, [&](const auto& first, const auto& second)
        {
            if (first.StartPhaseOffset != second.StartPhaseOffset)
                return first.StartPhaseOffset < second.StartPhaseOffset;

            const auto firstAtEnd = m_Notifies[first.NotifyIndex].Time == m_Duration;
            const auto secondAtEnd = m_Notifies[second.NotifyIndex].Time == m_Duration;
            return firstAtEnd && !secondAtEnd;
        });

        output = std::move(intervals);
        return true;
    }
}
