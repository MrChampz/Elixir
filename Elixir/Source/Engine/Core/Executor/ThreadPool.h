#pragma once

#include <Engine/Core/Executor/Thread.h>
#include <Engine/Core/Executor/WaitGroup.h>

namespace Elixir
{
    static unsigned GetNumHardwareThreads() {
        return std::thread::hardware_concurrency();
    }

    class ELIXIR_API ThreadPool
    {
        friend class WorkerThread;

      public:
        explicit ThreadPool(size_t numThreads = GetNumHardwareThreads());
        ThreadPool(ThreadPool const &) = delete;
        ThreadPool(ThreadPool &&) noexcept = delete;

        ~ThreadPool();

        template <typename F, typename... Args>
        void Enqueue(F&& func, Args&&... args, WaitGroup* wg = nullptr)
        {
            EE_CORE_ASSERT(m_Running, "ThreadPool is not running!")

            if (wg) wg->Add(1);

            Task task = [func = std::forward<F>(func), ... args = std::forward<Args>(args), wg]() mutable
            {
                try
                {
                    func(args...);
                }
                catch (...)
                {
                    // Ensure Done() is called even on exception
                    if (wg) wg->Done();
                    throw;
                }

                if (wg) wg->Done();
            };

            m_Queue.enqueue(std::move(task));
            m_Condition.notify_one();
        }

        bool IsRunning() const { return m_Running; }

        ThreadPool &operator=(ThreadPool const &) = delete;
        ThreadPool &operator=(ThreadPool &&) noexcept = delete;

      protected:
        Task GetTaskForWorker(size_t workerIndex);
        void WaitForWork();

      private:
        Task StealWork(size_t thiefIndex) const;

        std::vector<Scope<WorkerThread>> m_Workers;

        // Global queue
        moodycamel::ConcurrentQueue<Task> m_Queue;

        std::mutex m_Mutex;
        std::condition_variable m_Condition;

        std::atomic<bool> m_Running{true};
    };
}