#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <thread>
#include <vector>

#include <../../../Source/Engine/Aether/Rendering/SystemInstanceRetirementQueue.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;

TEST(SystemInstanceRetirementQueueTest, TransfersPendingInstancesExactlyOnce)
{
    const auto system = CreateRef<SCompiledSystem>();
    const auto first = CreateRef<SystemInstance>(system);
    const auto second = CreateRef<SystemInstance>(system);

    SystemInstanceRetirementQueue queue;
    queue.Enqueue(first);
    queue.Enqueue(second);

    const auto retired = queue.Drain();

    ASSERT_EQ(retired.size(), 2);
    EXPECT_EQ(retired[0], first);
    EXPECT_EQ(retired[1], second);
    EXPECT_TRUE(queue.Drain().empty());
}

TEST(SystemInstanceRetirementQueueTest, DrainsDestroyRequestsEnqueuedDuringUpdate)
{
    constexpr uint32_t destroyRequestCount = 256;
    const auto system = CreateRef<SCompiledSystem>();
    std::vector<Ref<SystemInstance>> instances;
    instances.reserve(destroyRequestCount);

    for (uint32_t instance = 0; instance < destroyRequestCount; ++instance)
        instances.push_back(CreateRef<SystemInstance>(system));

    SystemInstanceRetirementQueue queue;
    std::barrier beginConcurrentAccess{ 2 };
    std::atomic_bool producerFinished = false;
    size_t retiredCount = 0;

    std::thread destructionThread([&]
    {
        beginConcurrentAccess.arrive_and_wait();

        for (uint32_t request = 0; request < destroyRequestCount; ++request)
            queue.Enqueue(instances[request]);

        producerFinished.store(true, std::memory_order_release);
    });

    beginConcurrentAccess.arrive_and_wait();

    while (!producerFinished.load(std::memory_order_acquire))
    {
        retiredCount += queue.Drain().size();
        std::this_thread::yield();
    }

    destructionThread.join();
    retiredCount += queue.Drain().size();

    EXPECT_EQ(retiredCount, destroyRequestCount);
}
