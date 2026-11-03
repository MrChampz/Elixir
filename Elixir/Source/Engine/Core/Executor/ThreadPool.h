#pragma once

#include <Engine/Core/Executor/Thread.h>
#include <Engine/Core/Executor/Executable.h>
#include <Engine/Core/Executor/WaitGroup.h>

#include <latch>

namespace Elixir
{
    static unsigned GetNumHardwareThreads() {
        return std::thread::hardware_concurrency();
    }

    class ELIXIR_API ThreadPool
    {
        friend class Thread;

      public:
        /**
         * @brief Start workers after their pool and queues are initialized.
         * @param numThreads Worker count; zero selects the available hardware concurrency.
         * @param name Label used for logging and profiling.
         */
        explicit ThreadPool(
            size_t numThreads = GetNumHardwareThreads(),
            const std::string& name = ""
        );

        ThreadPool(ThreadPool const &) = delete;
        ThreadPool(ThreadPool &&) noexcept = delete;

        ~ThreadPool();

        /**
         * @brief Stop accepting tasks, finish queued work, and join all workers.
         * @pre Call from outside the pool and serialize shutdown with pool destruction.
         */
        void Shutdown();

        /**
         * @brief Queue a task and include it in an optional completion barrier.
         * @param executable Task to execute on a worker; must not throw.
         * @param group Optional barrier that remains alive until all its tasks complete.
         * @return False if the task is empty, the pool is stopped, or queuing fails.
         */
        bool TryEnqueue(Executable executable, WaitGroup* group = nullptr);

        /**
         * @brief Queue a callable and log an error if submission fails.
         * @tparam F Callable type.
         * @tparam Args Bound argument types.
         * @param func Callable to execute; must not throw.
         * @param args Arguments copied or moved into the task.
         * @param wg Optional barrier that remains alive until its tasks complete.
         */
        template <typename F, typename... Args>
        void Enqueue(F&& func, Args&&... args, WaitGroup* wg = nullptr)
        {
            Executable exec = [
                func = std::forward<F>(func),
                ... args = std::forward<Args>(args)
            ]() mutable
            {
                func(args...);
            };

            const auto queued = TryEnqueue(std::move(exec), wg);
            if (!queued)
                EE_CORE_ERROR("ThreadPool [{}] could not queue a task.", m_Name)
        }

        bool IsRunning() const { return m_Running; }

        [[nodiscard]] const std::string& GetName() const { return m_Name; }

        ThreadPool &operator=(ThreadPool const &) = delete;
        ThreadPool &operator=(ThreadPool &&) noexcept = delete;

      protected:
        Executable GetExecutableForWorker(size_t workerIndex);
        void WaitForWork();

      private:
        Executable StealWork(size_t thiefIndex) const;

        std::string m_Name;
    
        std::vector<Scope<Thread>> m_Workers;

        // Global queue
        moodycamel::ConcurrentQueue<Executable> m_Queue;

        std::mutex m_Mutex;
        std::condition_variable m_Condition;

        std::atomic<bool> m_Running{ true };
        std::latch m_WorkersReady{ 1 };
    };
}
