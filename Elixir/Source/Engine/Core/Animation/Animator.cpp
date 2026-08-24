#include "epch.h"
#include "Animator.h"

namespace Elixir
{
    Animator::AnimationId Animator::Bind(
        const float duration,
        std::function<void(float)> apply,
        std::function<void()> onComplete
    )
    {
        EE_CORE_ASSERT(apply, "Animator::Bind requires an apply callback");

        const AnimationId id = m_NextId++;
        if (duration <= 0.0f)
        {
            apply(0.0f);
            if (onComplete) onComplete();
            return id;
        }

        apply(0.0f);
        STrack track{
            .Id = id,
            .Duration = duration,
            .Apply = std::move(apply),
            .OnComplete = std::move(onComplete),
        };
        if (m_IsUpdating)
            m_PendingTracks.push_back(std::move(track));
        else
            m_Tracks.push_back(std::move(track));
        return id;
    }

    void Animator::Stop(const AnimationId id)
    {
        if (m_IsUpdating)
        {
            for (auto& track : m_Tracks)
            {
                if (track.Id == id)
                    track.Cancelled = true;
            }
            std::erase_if(m_PendingTracks, [id](const STrack& track) { return track.Id == id; });
            return;
        }

        std::erase_if(m_Tracks, [id](const STrack& track) { return track.Id == id; });
    }

    void Animator::StopAll()
    {
        if (m_IsUpdating)
        {
            for (auto& track : m_Tracks)
                track.Cancelled = true;
            m_PendingTracks.clear();
            return;
        }

        m_Tracks.clear();
    }

    void Animator::Update(const Timestep frameTime)
    {
        const float delta = std::max(0.0f, frameTime.GetSeconds());
        m_IsUpdating = true;
        for (auto& track : m_Tracks)
        {
            if (track.Cancelled) continue;

            track.Elapsed = std::min(track.Elapsed + delta, track.Duration);
            const float elapsed = track.Elapsed;
            const auto apply = track.Apply;
            apply(elapsed);
            if (track.Cancelled || track.Elapsed < track.Duration) continue;

            track.Completed = true;
            const auto onComplete = std::move(track.OnComplete);
            if (onComplete) onComplete();
        }

        m_IsUpdating = false;
        std::erase_if(m_Tracks, [](const STrack& track) { return track.Cancelled || track.Completed; });
        m_Tracks.insert(
            m_Tracks.end(),
            std::make_move_iterator(m_PendingTracks.begin()),
            std::make_move_iterator(m_PendingTracks.end())
        );
        m_PendingTracks.clear();
    }

    bool Animator::IsAnimating() const
    {
        return !m_PendingTracks.empty() || std::ranges::any_of(m_Tracks, [](const STrack& track)
        {
            return !track.Cancelled && !track.Completed;
        });
    }
}
