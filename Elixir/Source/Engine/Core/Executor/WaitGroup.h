#pragma once

namespace Elixir
{
    class ELIXIR_API WaitGroup
    {
        friend class ThreadPool;
      public:
        WaitGroup() : m_Count(0) {}

        void Wait();
        bool WaitFor(std::chrono::milliseconds timeout);

        int GetCount() const;

      protected:
        void Add(int delta = 1);
        void Done();

      private:
        mutable std::mutex m_Mutex;
        std::condition_variable m_Condition;
        int m_Count;
    };
}