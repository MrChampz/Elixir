#pragma once

#include <Engine/Core/Timer.h>

namespace Elixir
{
    class FrameProfiler
    {
    public:
        FrameProfiler() : m_FrameCount(0), m_ElapsedTime(0.0f), m_FPS(0) {}

        void OnUpdate(const Timestep frameTime)
        {
            m_ElapsedTime += frameTime.GetSeconds();
            m_FrameCount++;

            if (m_ElapsedTime >= 1.0f)
            {
                m_FPS = m_FrameCount;
                m_FrameCount = 0;
                m_ElapsedTime = 0.0f;
            }
        }

        [[nodiscard]] int GetFPS() const { return m_FPS; }

    private:
        int m_FrameCount;
        float m_ElapsedTime;
        int m_FPS;
    };
}