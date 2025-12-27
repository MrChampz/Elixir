#include "epch.h"
#include "Thread.h"

#include <Engine/Core/Executor/ThreadPool.h>

namespace Elixir
{
    WorkerThread::WorkerThread(ThreadPool* pool, const size_t index, const std::string& name)
        : Thread(name, std::thread(&WorkerThread::WorkerLoop, this)),
          m_Pool(pool),
          m_WorkerIndex(index)
    {
        EE_CORE_TRACE("Worker thread created: {0}", m_Name)
    }

    void WorkerThread::Enqueue(Task task)
    {
        m_Queue.enqueue(std::move(task));
    }

    void WorkerThread::ClearQueue()
    {
        Task discarted;
        while (m_Queue.try_dequeue(discarted)) {}
    }

    Task WorkerThread::StealTask()
    {
        Task task;
        if (m_Queue.try_dequeue(task))
        {
            return task;
        }

        return nullptr;
    }

    void WorkerThread::WorkerLoop()
    {
        EE_CORE_TRACE("Worker thread started: {0}", m_Name)
        tracy::SetThreadName(m_Name.c_str());

        while (m_Pool->IsRunning())
        {
            if (const auto task = AcquireTask())
                task();
        }
    }

    Task WorkerThread::AcquireTask()
    {
        Task task;

        // Try the local queue first
        if (m_Queue.try_dequeue(task))
        {
            return task;
        }

        // Delegate to ThreadPool for global/stealing logic.
        task = m_Pool->GetTaskForWorker(m_WorkerIndex);

        // If no task, wait briefly.
        if (!task)
            m_Pool->WaitForWork();

        return task;
    }
}