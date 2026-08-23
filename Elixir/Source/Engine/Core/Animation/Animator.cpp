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
        m_Tracks.push_back({
            .Id = id,
            .Duration = duration,
            .Apply = std::move(apply),
            .OnComplete = std::move(onComplete),
        });
        return id;
    }

    void Animator::Stop(const AnimationId id)
    {
        std::erase_if(m_Tracks, [id](const STrack& track) { return track.Id == id; });
    }

    void Animator::StopAll()
    {
        m_Tracks.clear();
    }

    void Animator::Update(const Timestep frameTime)
    {
        const float delta = std::max(0.0f, frameTime.GetSeconds());
        for (auto it = m_Tracks.begin(); it != m_Tracks.end();)
        {
            it->Elapsed = std::min(it->Elapsed + delta, it->Duration);
            it->Apply(it->Elapsed);

            if (it->Elapsed < it->Duration)
            {
                ++it;
                continue;
            }

            const auto onComplete = std::move(it->OnComplete);
            it = m_Tracks.erase(it);
            if (onComplete) onComplete();
        }
    }
}
