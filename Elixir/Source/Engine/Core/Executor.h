#pragma once

#include <ftl/task.h>
#include <ftl/task_scheduler.h>
#include <ftl/thread_abstraction.h>

#include <utility>

namespace Elixir
{
    class Executor;

    using TaskPriority = ftl::TaskPriority;
    using TaskFunction = void (*)(ftl::TaskScheduler* scheduler, void* arg);

    struct Task {
        TaskFunction Function;
        void* ArgData;
    };

    using ThreadRoutine = void* (*)(void* arg);

    class ELIXIR_API Thread
    {
        friend class Executor;

    public:
        [[nodiscard]] const std::string& GetName() const { return m_Name; }

    protected:
        explicit Thread(std::string name, const ftl::ThreadType thread)
            : m_Name(std::move(name)), m_Thread(thread) {}

        std::string m_Name;
        ftl::ThreadType m_Thread;
    };

    class ELIXIR_API WaitGroup
    {
        friend class Executor;

    public:
        explicit WaitGroup(Executor* executor);

        WaitGroup(std::initializer_list<int>) = delete;
        WaitGroup() = delete;

        /**
         * @brief Wait blocks until the WaitGroup counter is zero
         *
         * @param pinToCurrentThread    If true, this fiber won't be resumed on another thread
         */
        void Wait(bool pinToCurrentThread = false);

    protected:
        ftl::WaitGroup m_WaitGroup;
    };

    struct ExecutorInitOptions
    {
        /* The size of the fiber pool.The fiber pool is used to run new tasks when the current task is waiting on a counter */
        unsigned FiberPoolSize = 400;
        /* The size of the thread pool to run. 0 corresponds to the maximum of hardware threads */
        unsigned ThreadPoolSize = 0;
    };

    class ELIXIR_API Executor
    {
        friend class WaitGroup;

    public:
        Executor() = default;

        /**
         * Initializes the Executor and binds the current thread as the "main" thread
         *
         * Executor functions can *only* be called from this thread or
         * inside tasks on the worker threads.
         *
         * @param options    The configuration options for the Executor.
         * @return           Boolean indicating if it was succeeded.
         */
        bool Init(ExecutorInitOptions options = ExecutorInitOptions());

        void AddTask(Task task, TaskPriority priority, WaitGroup* waitGroup = nullptr);

        void AddTasks(
            uint32_t numTasks,
            Task* tasks,
            TaskPriority priority,
            WaitGroup* waitGroup = nullptr
        );

        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

        static Thread CreateThread(
            size_t stackSize,
            ThreadRoutine routine,
            void* args,
            const char* name
        );

        static Thread CreateThread(
            size_t stackSize,
            ThreadRoutine routine,
            void* args,
            const char* name,
            size_t coreAffinity
        );

        static bool JoinThread(const Thread& thread);

    protected:
        ftl::TaskScheduler m_Scheduler{};

    private:
        bool m_Initialized = false;
    };
}