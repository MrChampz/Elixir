#pragma once

#include <Engine/Core/Executor/ThreadPool.h>

namespace Elixir
{
    /**
     * Executor manages the main, rendering, and worker threads and workload.
     */
    class ELIXIR_API Executor
    {
      public:
        Executor(const Executor&) = delete;

        static Executor& Get()
        {
            static Executor s_Executor;
            return s_Executor;
        }

        template <typename F, typename... Args>
        void Enqueue(F&& func, Args&&... args, WaitGroup* wg = nullptr)
        {
            m_ThreadPool->Enqueue(func, args..., wg);
        }

        Executor& operator=(const Executor&) = delete;

      private:
        Executor()
        {
            m_ThreadPool = CreateScope<ThreadPool>();
        }

        Scope<ThreadPool> m_ThreadPool;
    };
}