#include "epch.h"
#include "Executor.h"

namespace Elixir
{
    void Executor::Init(const ExecutorInitOptions options)
    {
        if (!m_Initialized)
        {
            m_ThreadPool = CreateScope<ThreadPool>(options.ThreadPoolSize);
            m_Initialized = true;
        }
    }
}