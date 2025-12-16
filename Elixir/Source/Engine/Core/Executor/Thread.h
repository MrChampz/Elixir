#pragma once

#include <Engine/Core/Executor/Task.h>

#include <concurrentqueue.h>

namespace Elixir
{
    class ThreadPool;

    class ELIXIR_API Thread
    {
        friend class ThreadPool;

      public:
        explicit Thread(const std::string& name, std::thread thread)
            : m_Name(name), m_Thread(std::move(thread)) {}

        virtual ~Thread()
        {
            if (m_Thread.joinable())
            {
                m_Thread.join();
                EE_CORE_TRACE("Thread joined: {}", m_Name)
            }
        }

        [[nodiscard]] const std::string& GetName() const { return m_Name; }

      protected:
        std::string m_Name;
        std::thread m_Thread;
    };

    class ELIXIR_API WorkerThread final : public Thread
    {
        friend class ThreadPool;

      public:
        explicit WorkerThread(ThreadPool* pool, size_t index, const std::string& name);

        void Enqueue(Task task);

      protected:
        Task StealTask();

      private:
        void WorkerLoop();
        Task AcquireTask();

        moodycamel::ConcurrentQueue<Task> m_Queue;

        ThreadPool* m_Pool;
        size_t m_WorkerIndex;
    };
}