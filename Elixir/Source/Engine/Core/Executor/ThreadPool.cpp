#include "epch.h"
#include "ThreadPool.h"

namespace Elixir
{
    ThreadPool::ThreadPool(const size_t numThreads, const std::string& name)
        : m_Name(name)
    {
        const auto threads = numThreads == 0
            ? GetNumHardwareThreads()
            : numThreads;

        m_Workers.reserve(threads);

        for (size_t i = 0; i < threads; ++i)
        {
            auto worker = CreateScope<Thread>(this, i, "Worker " + std::to_string(i));
            m_Workers.push_back(std::move(worker));
        }
    }

    ThreadPool::~ThreadPool()
    {
        if (m_Running)
        {
            Shutdown();
        }
    }

    void ThreadPool::Shutdown()
    {
        if (!m_Running)
            return;

        m_Running = false;
        m_Condition.notify_all();

        for (const auto& worker : m_Workers)
            worker->Join();

        Executable discarted;
        while (m_Queue.try_dequeue(discarted)) {}
    }

    Executable ThreadPool::GetExecutableForWorker(const size_t workerIndex)
    {
        Executable executable;

        // Try global queue
        if (m_Queue.try_dequeue(executable))
        {
            return executable;
        }

        // Try stealing from other workers (FIFO)
        executable = StealWork(workerIndex);

        return executable;
    }

    void ThreadPool::WaitForWork()
    {
        std::unique_lock lock(m_Mutex);
        m_Condition.wait_for(lock, std::chrono::milliseconds(10), [this]
        {
            return !m_Running || m_Queue.size_approx() > 0;
        });
    }

    Executable ThreadPool::StealWork(const size_t thiefIndex) const
    {
        // Try stealing from other workers in round-robin fashion
        for (size_t i = 1; i < m_Workers.size(); ++i)
        {
            const auto victimIndex = (thiefIndex + i) % m_Workers.size();

            if (auto exec = m_Workers[victimIndex]->StealWork())
                return exec;
        }

        return nullptr;
    }
}