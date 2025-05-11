#pragma once

namespace Elixir
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    class ELIXIR_API Timestep
    {
    public:
        explicit Timestep(const float time = 0.0f) : m_Time(time) {}

        [[nodiscard]] float GetSeconds() const { return m_Time; }
        [[nodiscard]] float GetMilliseconds() const { return m_Time * 1000.0f; }

        explicit operator float() const { return m_Time; }

    private:
        float m_Time;
    };

    class ELIXIR_API Timer
    {
    public:
        explicit Timer(const TimePoint initialTime = Clock::now())
            : m_InitialTime(initialTime), m_LastFrameTime(initialTime) {}

        [[nodiscard]] Timestep GetTotalTime() const
        {
            const auto time = std::chrono::high_resolution_clock::now();
            const std::chrono::duration<float> delta = time - m_InitialTime;
            return Timestep(delta.count());
        }

        [[nodiscard]] Timestep GetLastFrameTime()
        {
            const auto time = std::chrono::high_resolution_clock::now();
            const std::chrono::duration<float> delta = time - m_LastFrameTime;
            m_LastFrameTime = time;
            return Timestep(delta.count());
        }

        [[nodiscard]] TimePoint GetInitialTime() const { return m_InitialTime; }

    private:
        TimePoint m_InitialTime;
        TimePoint m_LastFrameTime;
    };
}