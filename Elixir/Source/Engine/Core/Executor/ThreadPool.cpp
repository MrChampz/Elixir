#include "epch.h"
#include "ThreadPool.h"

namespace Elixir
{
    ThreadPool::ThreadPool(const size_t numThreads)
    {
        const auto threads = numThreads == 0
            ? GetNumHardwareThreads()
            : numThreads;

        m_Workers.reserve(threads);

        for (size_t i = 0; i < threads; ++i)
        {
            auto worker = CreateScope<WorkerThread>(this, i, "Worker " + std::to_string(i));
            m_Workers.push_back(std::move(worker));
        }
    }

    ThreadPool::~ThreadPool()
    {
        m_Running = false;
        m_Condition.notify_all();
    }

    Task ThreadPool::GetTaskForWorker(const size_t workerIndex)
    {
        Task task;

        // Try global queue
        if (m_Queue.try_dequeue(task))
        {
            return task;
        }

        // Try stealing from other workers (FIFO)
        task = StealWork(workerIndex);

        return task;
    }

    void ThreadPool::WaitForWork()
    {
        std::unique_lock lock(m_Mutex);
        m_Condition.wait_for(lock, std::chrono::milliseconds(10), [this]
        {
            return !m_Running || m_Queue.size_approx() > 0;
        });
    }

    Task ThreadPool::StealWork(const size_t thiefIndex) const
    {
        // Try stealing from other workers in round-robin fashion
        for (size_t i = 1; i < m_Workers.size(); ++i)
        {
            const auto victimIndex = (thiefIndex + i) % m_Workers.size();

            if (auto task = m_Workers[victimIndex]->StealTask())
                return task;
        }

        return nullptr;
    }
}