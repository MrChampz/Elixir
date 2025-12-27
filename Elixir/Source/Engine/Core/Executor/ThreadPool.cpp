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
        if (m_Running)
        {
            Task discarted;
            while (m_Queue.try_dequeue(discarted)) {}

            for (const auto&    worker : m_Workers)
                worker->ClearQueue();
        }

        while (m_ActiveTasks.load() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        m_Running = false;
        m_Condition.notify_all();
    }

    void ThreadPool::Shutdown()
    {
        // Aguardar tarefas ativas terminarem
        while (m_ActiveTasks.load() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Parar as threads
        m_Running = false;
        m_Condition.notify_all();

        // Unir as threads dos workers
        for (auto& worker : m_Workers)
        {
            worker->Join();  // Assumindo que WorkerThread::Join() faz join na std::thread
        }
    }

    void ThreadPool::WaitForAllTasks() const
    {
        while (m_ActiveTasks.load() > 0)
        {
            std::this_thread::yield();
        }
    }

    Task ThreadPool::GetTaskForWorker(const size_t workerIndex)
    {
        Task task;

        // Try global queue
        if (m_Queue.try_dequeue(task))
        {
            if (task)
            {
                m_ActiveTasks.fetch_add(1);

                return [this, originalTask = std::move(task)]() mutable
                {
                    try
                    {
                        originalTask();
                    }
                    catch (...)
                    {
                        m_ActiveTasks.fetch_sub(1);
                        throw;
                    }

                    m_ActiveTasks.fetch_sub(1);
                };
            }
        }

        // Try stealing from other workers (FIFO)
        task = StealWork(workerIndex);

        if (task)
        {
            m_ActiveTasks.fetch_add(1);

            return [this, originalTask = std::move(task)]() mutable
            {
                try
                {
                    originalTask();
                }
                catch (...)
                {
                    m_ActiveTasks.fetch_sub(1);
                    throw;
                }

                m_ActiveTasks.fetch_sub(1);
            };
        }

        return nullptr;
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