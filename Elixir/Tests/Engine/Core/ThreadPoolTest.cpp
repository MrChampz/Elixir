#include <gtest/gtest.h>
#include <Engine/Core/Executor/ThreadPool.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>

using namespace Elixir;

TEST(ThreadPoolTest, ShutdownCompletesAcceptedJobsAndReleasesTheirWaitGroup)
{
    ThreadPool workers(1, "ShutdownTests");
    WaitGroup completion;
    std::latch release{1};
    std::latch started{1};
    std::atomic<size_t> completed{0};
    auto holdWorker = [&]
    {
        started.count_down();
        release.wait();
        completed.fetch_add(1);
    };
    ASSERT_TRUE(workers.TryEnqueue(holdWorker, &completion));
    started.wait();
    for (size_t i = 0; i < 16; ++i)
    {
        auto task = [&completed] { completed.fetch_add(1); };
        EXPECT_TRUE(workers.TryEnqueue(task, &completion));
    }

    std::thread shutdown([&workers] { workers.Shutdown(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (workers.IsRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const auto stoppedAccepting = !workers.IsRunning();
    release.count_down();
    shutdown.join();

    EXPECT_TRUE(stoppedAccepting);
    EXPECT_EQ(completed.load(), 17);
    EXPECT_TRUE(completion.WaitFor(std::chrono::milliseconds(10)));
    EXPECT_EQ(completion.GetCount(), 0);
    EXPECT_FALSE(workers.TryEnqueue([] {}, &completion));
    EXPECT_EQ(completion.GetCount(), 0);
}

TEST(ThreadPoolTest, EmptyTasksDoNotChangeTheCompletionCount)
{
    ThreadPool workers(1, "EmptyTaskTests");
    WaitGroup completion;
    EXPECT_FALSE(workers.TryEnqueue({}, &completion));
    EXPECT_EQ(completion.GetCount(), 0);
}

TEST(ThreadPoolTest, ShutdownDrainsTasksQueuedBeforeIdleWorkersWake)
{
    ThreadPool workers(2, "IdleShutdownTests");
    WaitGroup completion;
    std::atomic<size_t> completed{0};
    for (size_t i = 0; i < 64; ++i)
    {
        auto task = [&completed] { completed.fetch_add(1); };
        ASSERT_TRUE(workers.TryEnqueue(task, &completion));
    }
    workers.Shutdown();
    EXPECT_EQ(completed.load(), 64);
    EXPECT_TRUE(completion.WaitFor(std::chrono::milliseconds(10)));
}
