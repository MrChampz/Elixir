#pragma once

#include <Engine/Core/Executor/Thread.h>
#include <Engine/Core/Executor/Executable.h>
#include <Engine/Core/Executor/WaitGroup.h>

namespace Elixir
{
    static unsigned GetNumHardwareThreads() {
        return std::thread::hardware_concurrency();
    }

    class ELIXIR_API ThreadPool
    {
        friend class Thread;

      public:
        explicit ThreadPool(size_t numThreads = GetNumHardwareThreads(), const std::string& name = nullptr);
        ThreadPool(ThreadPool const &) = delete;
        ThreadPool(ThreadPool &&) noexcept = delete;

        ~ThreadPool();

        void Shutdown();

        template <typename F, typename... Args>
        void Enqueue(F&& func, Args&&... args, WaitGroup* wg = nullptr)
        {
            EE_CORE_ASSERT(m_Running, "ThreadPool [{0}] is not running!", m_Name)

            if (wg) wg->Add(1);

            Executable exec = [func = std::forward<F>(func), ... args = std::forward<Args>(args), wg]() mutable
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

            m_Queue.enqueue(std::move(exec));
            m_Condition.notify_one();
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

        std::atomic<bool> m_Running{true};
    };
}