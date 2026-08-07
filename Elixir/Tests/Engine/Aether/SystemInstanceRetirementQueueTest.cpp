#include <gtest/gtest.h>

#include <Engine/Aether/SystemInstanceRetirementQueue.h>

using namespace Elixir;
using namespace Elixir::Aether;

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