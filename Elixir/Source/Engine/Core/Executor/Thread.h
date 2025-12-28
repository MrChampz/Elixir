#pragma once

#include <Engine/Core/Executor/Executable.h>

#include <concurrentqueue.h>

namespace Elixir
{
    class ThreadPool;

    class ELIXIR_API Thread final
    {
        friend class ThreadPool;

      public:
        explicit Thread(ThreadPool* pool, size_t index, const std::string& name);
        Thread(
          ThreadPool* pool,
          size_t index,
          const std::string& name,
          std::thread thread
        );

        ~Thread();
        
        void Enqueue(Executable executable);
        void Join();

        [[nodiscard]] const std::string& GetName() const { return m_Name; }

      protected:
        Executable StealWork();

      private:
        void WorkerLoop();
        Executable AcquireWork();

        ThreadPool* m_Pool;
        size_t m_WorkerIndex;

        std::string m_Name;
        std::thread m_Thread;
        moodycamel::ConcurrentQueue<Executable> m_Queue;
    };
}