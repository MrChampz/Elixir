#include "epch.h"
#include "WidgetAnimation.h"

namespace Elixir::GUI
{
    WidgetAnimation::WidgetAnimation(Ref<Widget> target)
      : m_Target(std::move(target))
    {
        EE_CORE_ASSERT(m_Target, "WidgetAnimation requires a target widget");
    }

    void WidgetAnimation::ClearTracks()
    {
        Stop();
        m_Tracks.clear();
    }

    void WidgetAnimation::Play()
    {
        Stop();
        if (!m_Target || m_Tracks.empty())
        {
            if (m_OnFinished) m_OnFinished();
            return;
        }

        m_IsPlaying = true;
        m_PendingTracks = m_Tracks.size();
        for (const TTrack& track : m_Tracks)
        {
            track(m_Animator, m_Target, [this]
            {
                if (--m_PendingTracks != 0) return;

                m_IsPlaying = false;
                if (m_OnFinished) m_OnFinished();
            });
        }
    }

    void WidgetAnimation::Stop()
    {
        m_Animator.StopAll();
        m_PendingTracks = 0;
        m_IsPlaying = false;
    }

    void WidgetAnimation::Update(const Timestep frameTime)
    {
        m_Animator.Update(frameTime);
    }
}
