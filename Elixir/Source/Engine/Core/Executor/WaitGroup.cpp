#include "epch.h"
#include "WaitGroup.h"

namespace Elixir
{
    void WaitGroup::Wait()
    {
        std::unique_lock lock(m_Mutex);
        m_Condition.wait(lock, [this] { return m_Count <= 0; });
    }

    bool WaitGroup::WaitFor(const std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(m_Mutex);
        return m_Condition.wait_for(lock, timeout, [this] { return m_Count <= 0; });
    }

    int WaitGroup::GetCount() const
    {
        std::lock_guard lock(m_Mutex);
        return m_Count;
    }

    void WaitGroup::Add(const int delta)
    {
        std::lock_guard lock(m_Mutex);
        m_Count += delta;
    }

    void WaitGroup::Done()
    {
        std::unique_lock lock(m_Mutex);
        if (--m_Count <= 0)
            m_Condition.notify_all();
    }
}