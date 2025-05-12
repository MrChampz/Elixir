#include "epch.h"
#include "Executor.h"

#ifdef EE_DEBUG
#include <tracy/Tracy.hpp>
#endif

struct ThreadRoutineData
{
    Elixir::ThreadRoutine Routine;
    void* Args;
};

#ifdef _WIN32
    unsigned int __stdcall ThreadRoutineTrampoline(void* arg)
    {
        const auto data = static_cast<ThreadRoutineData*>(arg);
        data->Routine(data->Args);
        delete data;
        return 0;
    }
#else
    void* ThreadRoutineTrampoline(void* arg)
    {
        const auto data = static_cast<ThreadRoutineData*>(arg);
        const auto result = data->Routine(data->Args);
        delete data;
        return result;
    }
#endif

namespace Elixir
{
    /**
     * @brief Called when a fiber is attached to a thread
     *
     * @param context       EventCallbacks::Context
     * @param fiberIndex    The index of the fiber
     */
    void OnFiberAttached(void* context, const unsigned fiberIndex)
    {
        // const auto name = std::to_string(fiberIndex);
        //const auto fiberId = new char[1];
        //snprintf(fiberId, 1, "%i", fiberIndex);
        //TracyFiberEnter(fiberId);
    }

    /**
     * @brief Called when a fiber is detached from a thread
     *
     * @param context       EventCallbacks::Context
     * @param fiberIndex    The index of the fiber
     * @param isMidTask     True = the fiber was suspended mid-task due to a wait. False = the fiber was suspended for another reason
     */
    void OnFiberDetached(void* context, unsigned fiberIndex, bool isMidTask)
    {
        //TracyFiberLeave;
    }

    void OnWorkerThreadStarted(void* context, unsigned threadIndex)
    {
    }

    WaitGroup::WaitGroup(Executor* executor)
        : m_WaitGroup(&executor->m_Scheduler)
    {
        EE_CORE_ASSERT(executor->IsInitialized(), "Executor was not initialized!")
    }

    void WaitGroup::Wait(bool pinToCurrentThread)
    {
        m_WaitGroup.Wait(pinToCurrentThread);
    }

    bool Executor::Init(const ExecutorInitOptions options)
    {
        if (!m_Initialized)
        {
            const auto result = m_Scheduler.Init({
                options.FiberPoolSize,
                options.ThreadPoolSize,
                ftl::EmptyQueueBehavior::Spin,
                {
                    .OnWorkerThreadStarted = OnWorkerThreadStarted,
                    .OnFiberAttached = OnFiberAttached,
                    .OnFiberDetached = OnFiberDetached,
                }
            });
            if (result == 0)
                m_Initialized = true;

            return m_Initialized;
        }
        return false;
    }

    void Executor::AddTask(
        const Task task,
        const TaskPriority priority,
        WaitGroup* waitGroup
    )
    {
        EE_CORE_ASSERT(IsInitialized(), "Executor was not initialized!")
        m_Scheduler.AddTask(
            { task.Function, task.ArgData },
            priority,
            &waitGroup->m_WaitGroup
        );
    }

    void Executor::AddTasks(
        const uint32_t numTasks,
        Task* tasks,
        const TaskPriority priority,
        WaitGroup* waitGroup
    )
    {
        EE_CORE_ASSERT(IsInitialized(), "Executor was not initialized!")
        m_Scheduler.AddTasks(
            numTasks,
            reinterpret_cast<ftl::Task*>(tasks),
            priority,
            &waitGroup->m_WaitGroup
        );
    }

    Thread Executor::CreateThread(
        const size_t stackSize,
        const ThreadRoutine routine,
        void* args,
        const char* name
    )
    {
        ftl::ThreadType thread;
        const auto data = new ThreadRoutineData{ routine, args };
        ftl::CreateThread(stackSize, ThreadRoutineTrampoline, data, name, &thread);
        return Thread(name, thread);
    }

    Thread Executor::CreateThread(
        const size_t stackSize,
        const ThreadRoutine routine,
        void* args,
        const char* name,
        const size_t coreAffinity
    )
    {
        ftl::ThreadType thread;
        const auto data = new ThreadRoutineData{ routine, args };
        ftl::CreateThread(stackSize, ThreadRoutineTrampoline, data, name, coreAffinity, &thread);
        return Thread(name, thread);
    }

    bool Executor::JoinThread(const Thread& thread)
    {
        const auto result = ftl::JoinThread(thread.m_Thread);
#ifdef EE_DEBUG
        if (result) tracy::SetThreadName(thread.m_Name.c_str());
#endif
        return result;
    }
}