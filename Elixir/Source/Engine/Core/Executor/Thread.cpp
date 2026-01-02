#include "epch.h"
#include "Thread.h"

#include <Engine/Core/Executor/ThreadPool.h>

namespace Elixir
{
    Thread::Thread(ThreadPool* pool, const size_t index, const std::string& name)
        : Thread(pool, index, name, std::thread(&Thread::WorkerLoop, this)) {}
    
    Thread::Thread(
        ThreadPool* pool,
        const size_t index,
        const std::string& name,
        std::thread thread
    ) : m_Pool(pool), m_WorkerIndex(index), m_Name(name), m_Thread(std::move(thread))
    {
        EE_CORE_TRACE("Thread created: [Name = {0}, ThreadPool = {1}]", m_Name, m_Pool->GetName())
    }

    Thread::~Thread()
    {
        Join();
    }

    void Thread::Enqueue(Executable executable)
    {
        m_Queue.enqueue(std::move(executable));
    }

    void Thread::Join()
    {
        if (m_Thread.joinable())
        {
            m_Thread.join();
            EE_CORE_TRACE("Thread joined: [Name = {0}, ThreadPool = {1}]", m_Name, m_Pool->GetName())
        }
    }

    Executable Thread::StealWork()
    {
        Executable executable;
        if (m_Queue.try_dequeue(executable))
        {
            return executable;
        }

        return nullptr;
    }

    void Thread::WorkerLoop()
    {
        EE_CORE_TRACE("Thread started: [Name = {0}, ThreadPool = {1}]", m_Name, m_Pool->GetName())

        std::ostringstream name;
        name << "[" << m_Pool->GetName() << "] " << m_Name;
        tracy::SetThreadName(name.str().c_str());

        while (m_Pool->IsRunning())
        {
            if (const auto executable = AcquireWork())
                executable();
        }
    }

    Executable Thread::AcquireWork()
    {
        Executable executable;

        // Try the local queue first
        if (m_Queue.try_dequeue(executable))
        {
            return executable;
        }

        // Delegate to ThreadPool for global/stealing logic.
        executable = m_Pool->GetExecutableForWorker(m_WorkerIndex);

        // If no work, wait briefly.
        if (!executable)
            m_Pool->WaitForWork();

        return executable;
    }
}