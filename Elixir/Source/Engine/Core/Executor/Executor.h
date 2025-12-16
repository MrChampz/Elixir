#pragma once

#include <Engine/Core/Executor/ThreadPool.h>

namespace Elixir
{
    class Executor;

    struct ExecutorInitOptions
    {
        /* The size of the thread pool to run. 0 corresponds to the maximum of hardware threads */
        unsigned ThreadPoolSize = 0;
    };

    class ELIXIR_API Executor
    {
      public:
        Executor() = default;

        /**
         * Initializes the Executor and binds the current thread as the "main" thread
         *
         * Executor functions can *only* be called from this thread or
         * inside tasks on the worker threads.
         *
         * @param options    The configuration options for the Executor.
         */
        void Init(ExecutorInitOptions options = ExecutorInitOptions());

        template <typename F, typename... Args>
        void AddTask(F&& func, Args&&... args, WaitGroup* wg = nullptr)
        {
            EE_CORE_ASSERT(IsInitialized(), "Executor was not initialized!")
            m_ThreadPool->Enqueue(func, args..., wg);
        }

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

      private:
        Scope<ThreadPool> m_ThreadPool;
        bool m_Initialized = false;
    };
}