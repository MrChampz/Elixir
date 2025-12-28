#pragma once

#include <Engine/Core/Executor/ThreadPool.h>

namespace Elixir
{
    enum class EThreadName : uint8_t
    {
        Rendering, Worker
    };

    /**
     * @class Executor
     * @brief Represents an abstraction for executing tasks or commands.
     *
     * The Executor class provides a framework for managing and executing tasks asynchronously.
     * It allows users to submit tasks, manage their execution lifecycle, and
     * handle concurrency efficiently.
     */
    class ELIXIR_API Executor
    {
      public:
        Executor(const Executor&) = delete;

        ~Executor()
        {
            Shutdown();
        }

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

        void ShutdownRenderPool()
        {
            if (m_RenderPool)
            {
                m_RenderPool->Shutdown();
                m_RenderPool.reset();
            }
        }

        void ShutdownWorkerPool()
        {
            if (m_WorkerPool)
            {
                m_WorkerPool->Shutdown();
                m_WorkerPool.reset();
            }
        }

        void Shutdown()
        {
            ShutdownRenderPool();
            ShutdownWorkerPool();
        }

        Executor& operator=(const Executor&) = delete;

      private:
        Executor()
        {
            m_RenderPool = CreateScope<ThreadPool>(1, "Rendering");

            // Max hardware threads subtracted by 1 render thread and 1 main thread.
            const auto workers = GetNumHardwareThreads() - 2;
            m_WorkerPool = CreateScope<ThreadPool>(workers, "Workers");
        }

        Scope<ThreadPool> m_RenderPool;
        Scope<ThreadPool> m_WorkerPool;
    };
}