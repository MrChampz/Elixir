#pragma once

#include <Engine/Core/Executor/ThreadPool.h>

namespace Elixir
{
    enum class EThreadName : uint8_t
    {
        Rendering, Worker
    };

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
            Enqueue(EThreadName::Worker, std::forward<F>(func), std::forward<Args>(args)..., wg);
        }

        template <typename F, typename... Args>
        void Enqueue(const EThreadName thread, F&& func, Args&&... args, WaitGroup* wg = nullptr)
        {
            if (thread == EThreadName::Rendering)
            {
                m_RenderPool->Enqueue(func, args..., wg);
                return;
            }

            m_WorkerPool->Enqueue(func, args..., wg);
        }

        void WaitForRenderPool() const
        {
            m_RenderPool->WaitForAllTasks();
        }

        void WaitForAllTasks() const
        {
            m_RenderPool->WaitForAllTasks();
            m_WorkerPool->WaitForAllTasks();
        }

        void Reset()
        {
            WaitForAllTasks();
            m_RenderPool.reset(new ThreadPool(1));
            m_WorkerPool.reset(new ThreadPool(GetNumHardwareThreads() - 2));
        }

        Executor& operator=(const Executor&) = delete;

      private:
        Executor()
        {
            m_RenderPool = CreateScope<ThreadPool>(1);

            // Num hardware threads subtracted by 1 render thread and 1 main thread.
            const auto workers = GetNumHardwareThreads() - 2;
            m_WorkerPool = CreateScope<ThreadPool>(workers);
        }

        Scope<ThreadPool> m_RenderPool;
        Scope<ThreadPool> m_WorkerPool;
    };
}