#include <gtest/gtest.h>

#include <Engine/Aether/ParticleResourcePool.h>

using namespace Elixir;
using namespace Elixir::Aether;

namespace
{
    SCompiledSystem MakeCompiledSystem(
        const uint32_t particleCount,
        const uint32_t emitterCount,
        const uint32_t opCount,
        const uint32_t parameterCount,
        const uint32_t triggerTargetCount
    )
    {
        SCompiledSystem system{};
        system.TotalMaxParticles = particleCount;
        system.Emitters.resize(emitterCount);
        system.Ops.resize(opCount);
        system.Parameters.resize(parameterCount);
        system.TriggerTargets.resize(triggerTargetCount);
        return system;
    }

    SParticlePoolLimits MakePoolLimits()
    {
        return {
            .MaxSystemInstances = 4,
            .ParticleCapacity = 64,
            .EmitterCapacity = 16,
            .OpCapacity = 32,
            .ParameterCapacity = 32,
            .TriggerTargetCapacity = 16,
            .TriggerEventCapacityPerEmitter = 4,
        };
    }
}

TEST(AetherParticleResourcePoolTest, AllocatesDisjointRangesForLiveInstances)
{
    ParticleResourcePool pool{ MakePoolLimits() };

    const auto first = pool.Allocate(MakeCompiledSystem(10, 2, 3, 4, 1));
    const auto second = pool.Allocate(MakeCompiledSystem(20, 3, 5, 6, 2));

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    EXPECT_NE(first->InstanceIndex, second->InstanceIndex);
    EXPECT_EQ(second->Particles.Offset, first->Particles.Count);
    EXPECT_EQ(second->Emitters.Offset, first->Emitters.Count);
    EXPECT_EQ(second->Ops.Offset, first->Ops.Count);
    EXPECT_EQ(second->Parameters.Offset, first->Parameters.Count);
    EXPECT_EQ(second->TriggerTargets.Offset, first->TriggerTargets.Count);
    EXPECT_EQ(second->EmitterStates.Offset, first->EmitterStates.Count);
    EXPECT_EQ(second->SpawnRequests.Offset, first->SpawnRequests.Count);
    EXPECT_EQ(second->TriggerEvents.Offset, first->TriggerEvents.Count);
    EXPECT_EQ(second->TriggerQueueStates.Offset, first->TriggerQueueStates.Count);
}

TEST(AetherParticleResourcePoolTest, ReusesReleasedRanges)
{
    ParticleResourcePool pool{ MakePoolLimits() };
    const auto system = MakeCompiledSystem(10, 2, 3, 4, 1);

    const auto first = pool.Allocate(system);
    ASSERT_TRUE(first);

    pool.Release(*first);

    const auto reused = pool.Allocate(system);
    ASSERT_TRUE(reused);

    EXPECT_EQ(reused->InstanceIndex, first->InstanceIndex);
    EXPECT_EQ(reused->Particles.Offset, first->Particles.Offset);
    EXPECT_EQ(reused->Emitters.Offset, first->Emitters.Offset);
    EXPECT_EQ(reused->Ops.Offset, first->Ops.Offset);
    EXPECT_EQ(reused->Parameters.Offset, first->Parameters.Offset);
    EXPECT_EQ(reused->TriggerTargets.Offset, first->TriggerTargets.Offset);
    EXPECT_EQ(reused->EmitterStates.Offset, first->EmitterStates.Offset);
    EXPECT_EQ(reused->SpawnRequests.Offset, first->SpawnRequests.Offset);
    EXPECT_EQ(reused->TriggerEvents.Offset, first->TriggerEvents.Offset);
    EXPECT_EQ(reused->TriggerQueueStates.Offset, first->TriggerQueueStates.Offset);
}

TEST(AetherParticleResourcePoolTest, RollsBackPartialAllocationFailure)
{
    auto limits = MakePoolLimits();
    limits.ParticleCapacity = 4;

    ParticleResourcePool pool{ limits };

    EXPECT_FALSE(pool.Allocate(MakeCompiledSystem(5, 1, 1, 1, 1)));

    const auto allocation = pool.Allocate(MakeCompiledSystem(4, 1, 1, 1, 1));
    ASSERT_TRUE(allocation);

    EXPECT_EQ(allocation->InstanceIndex, 0u);
    EXPECT_EQ(allocation->Particles.Offset, 0u);
    EXPECT_EQ(allocation->Emitters.Offset, 0u);
    EXPECT_EQ(allocation->Ops.Offset, 0u);
    EXPECT_EQ(allocation->Parameters.Offset, 0u);
    EXPECT_EQ(allocation->TriggerTargets.Offset, 0u);
    EXPECT_EQ(allocation->EmitterStates.Offset, 0u);
    EXPECT_EQ(allocation->SpawnRequests.Offset, 0u);
    EXPECT_EQ(allocation->TriggerEvents.Offset, 0u);
    EXPECT_EQ(allocation->TriggerQueueStates.Offset, 0u);
}
