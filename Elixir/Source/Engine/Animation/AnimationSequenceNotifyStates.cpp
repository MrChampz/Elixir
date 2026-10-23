#include "epch.h"
#include "AnimationSequence.h"

namespace Elixir
{
    Ref<AnimationSequence> AnimationSequence::WithNotifyStates(
        std::vector<SAnimationNotifyState> notifyStates
    ) const
    {
        return Create(
            m_Name,
            m_Skeleton,
            m_Duration,
            m_Tracks,
            m_SyncMarkers,
            m_Notifies,
            std::move(notifyStates)
        );
    }

    bool AnimationSequence::CollectNotifyStateIntervals(
        const double startPhase,
        double endPhase,
        const bool looping,
        std::vector<SAnimationNotifyStateInterval>& output,
        const AnimationSequence* reference
    ) const
    {
        if (!std::isfinite(startPhase) ||
            !std::isfinite(endPhase) ||
            startPhase < 0 ||
            startPhase > 1 ||
            endPhase < startPhase)
        {
            EE_CORE_ERROR("Notify state collection requires a finite forward phase interval.")
            return false;
        }

        if (!looping)
            endPhase = std::min(endPhase, 1.0);

        if (startPhase == endPhase || m_NotifyStates.empty())
        {
            output.clear();
            return true;
        }

        if (endPhase > 0x1p52)
        {
            EE_CORE_ERROR("Notify state collection exceeded the supported phase range.")
            return false;
        }

        const auto synchronized = reference && CanSynchronizeWith(*reference);
        std::vector<SAnimationNotifyStateInterval> intervals;

        for (size_t index = 0; index < m_NotifyStates.size(); ++index)
        {
            const auto& state = m_NotifyStates[index];
            auto start = double(state.StartTime) / m_Duration;
            auto end = double(state.EndTime) / m_Duration;

            if (synchronized)
            {
                start = reference->GetSynchronizedPhase(start, *this);
                end = reference->GetSynchronizedPhase(end, *this);

                const auto cycle = std::floor(start);
                start -= cycle;
                end -= cycle;
            }

            const auto firstCycle = std::floor(startPhase - end) + 1;
            auto lastCycle = std::floor(endPhase - start);

            if (!looping && start + lastCycle >= 1)
                lastCycle -= 1;

            const auto count = std::max(0.0, lastCycle - firstCycle + 1);
            if (count > double(MaxNotifyEventsPerUpdate - intervals.size()))
            {
                EE_CORE_ERROR("Animation update exceeded the notify state interval capacity.")
                return false;
            }

            for (size_t i = 0; i < size_t(count); ++i)
            {
                const auto begin = start + firstCycle + double(i);
                const auto finish = end + firstCycle + double(i);

                if (!looping && begin >= 1)
                    continue;

                intervals.push_back({
                    index,
                    std::max(startPhase, begin) - startPhase,
                    std::min(endPhase, finish) - startPhase,
                    finish <= endPhase
                });
            }
        }

        std::ranges::stable_sort(
            intervals,
            {},
            &SAnimationNotifyStateInterval::StartPhaseOffset
        );

        output = std::move(intervals);
        return true;
    }
}
