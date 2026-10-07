#include <gtest/gtest.h>

#include <Engine/Aether/Simulation/ResourcePool.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Core;

namespace
{
    SCompiledSystem MakeCompiledSystem(
        const uint32_t particleCount,
        const uint32_t emitterCount,
        const uint32_t operationCount,
        const uint32_t parameterCount,
        const uint32_t triggerTargetCount
    )
    {
        SCompiledSystem system{};
        system.TotalMaxParticles = particleCount;
        system.Emitters.resize(emitterCount);
        system.Operations.Operations.resize(operationCount);
        system.Operations.Operands.resize(operationCount * 2);
        system.Operations.Constants.resize(operationCount * 3);
        system.Parameters.resize(parameterCount);
        system.TriggerTargets.resize(triggerTargetCount);
        return system;
    }

    SResourcePoolLimits MakePoolLimits()
    {
        return {
            .MaxSystemInstances = 4,
            .ParticleCapacity = 64,
            .EmitterCapacity = 16,
            .OperationCapacity = 32,
            .ParameterCapacity = 32,
            .TriggerTargetCapacity = 16,
            .TriggerEventCapacityPerEmitter = 4,
        };
    }
}

TEST(ResourcePoolTest, AllocatesDisjointRangesForLiveInstances)
{
    const auto limits = MakePoolLimits();
    const ParticleStateLayoutRegistry layouts{ limits.ParticleCapacity };
    ResourcePool pool{ limits, layouts };

    const auto first = pool.Allocate(MakeCompiledSystem(10, 2, 3, 4, 1));
    const auto second = pool.Allocate(MakeCompiledSystem(20, 3, 5, 6, 2));

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    EXPECT_NE(first->InstanceIndex, second->InstanceIndex);
    EXPECT_EQ(first->ParticleStateLayout, EParticleStateLayout::CoreV1);
    EXPECT_EQ(second->ParticleStateLayout, EParticleStateLayout::CoreV1);
    EXPECT_EQ(second->Particles.Offset, first->Particles.Count);
    EXPECT_EQ(second->Emitters.Offset, first->Emitters.Count);
    EXPECT_EQ(second->Operations.Offset, first->Operations.Count);
    EXPECT_EQ(second->Operands.Offset, first->Operands.Count);
    EXPECT_EQ(second->Constants.Offset, first->Constants.Count);
    EXPECT_EQ(second->Parameters.Offset, first->Parameters.Count);
    EXPECT_EQ(second->TriggerTargets.Offset, first->TriggerTargets.Count);
    EXPECT_EQ(second->EmitterStates.Offset, first->EmitterStates.Count);
    EXPECT_EQ(second->SpawnRequests.Offset, first->SpawnRequests.Count);
    EXPECT_EQ(second->TriggerEvents.Offset, first->TriggerEvents.Count);
    EXPECT_EQ(second->TriggerQueueStates.Offset, first->TriggerQueueStates.Count);
}

TEST(ResourcePoolTest, ReusesReleasedRanges)
{
    const auto limits = MakePoolLimits();
    const ParticleStateLayoutRegistry layouts{ limits.ParticleCapacity };
    ResourcePool pool{ limits, layouts };
    const auto system = MakeCompiledSystem(10, 2, 3, 4, 1);

    const auto first = pool.Allocate(system);
    ASSERT_TRUE(first);

    pool.Release(*first);

    const auto reused = pool.Allocate(system);
    ASSERT_TRUE(reused);

    EXPECT_EQ(reused->InstanceIndex, first->InstanceIndex);
    EXPECT_NE(reused->Generation, first->Generation);
    EXPECT_EQ(reused->Particles.Offset, first->Particles.Offset);
    EXPECT_EQ(reused->Emitters.Offset, first->Emitters.Offset);
    EXPECT_EQ(reused->Operations.Offset, first->Operations.Offset);
    EXPECT_EQ(reused->Operands.Offset, first->Operands.Offset);
    EXPECT_EQ(reused->Constants.Offset, first->Constants.Offset);
    EXPECT_EQ(reused->Parameters.Offset, first->Parameters.Offset);
    EXPECT_EQ(reused->TriggerTargets.Offset, first->TriggerTargets.Offset);
    EXPECT_EQ(reused->EmitterStates.Offset, first->EmitterStates.Offset);
    EXPECT_EQ(reused->SpawnRequests.Offset, first->SpawnRequests.Offset);
    EXPECT_EQ(reused->TriggerEvents.Offset, first->TriggerEvents.Offset);
    EXPECT_EQ(reused->TriggerQueueStates.Offset, first->TriggerQueueStates.Offset);
}

TEST(ResourcePoolTest, RollsBackPartialAllocationFailure)
{
    auto limits = MakePoolLimits();
    limits.ParticleCapacity = 4;

    const ParticleStateLayoutRegistry layouts{ limits.ParticleCapacity };
    ResourcePool pool{ limits, layouts };

    EXPECT_FALSE(pool.Allocate(MakeCompiledSystem(5, 1, 1, 1, 1)));

    const auto allocation = pool.Allocate(MakeCompiledSystem(4, 1, 1, 1, 1));
    ASSERT_TRUE(allocation);

    EXPECT_EQ(allocation->InstanceIndex, 0u);
    EXPECT_EQ(allocation->Particles.Offset, 0u);
    EXPECT_EQ(allocation->Emitters.Offset, 0u);
    EXPECT_EQ(allocation->Operations.Offset, 0u);
    EXPECT_EQ(allocation->Parameters.Offset, 0u);
    EXPECT_EQ(allocation->TriggerTargets.Offset, 0u);
    EXPECT_EQ(allocation->EmitterStates.Offset, 0u);
    EXPECT_EQ(allocation->SpawnRequests.Offset, 0u);
    EXPECT_EQ(allocation->TriggerEvents.Offset, 0u);
    EXPECT_EQ(allocation->TriggerQueueStates.Offset, 0u);
}

TEST(ResourcePoolTest, RejectsAnUnregisteredParticleStateLayout)
{
    const auto limits = MakePoolLimits();
    const ParticleStateLayoutRegistry layouts{ limits.ParticleCapacity };
    ResourcePool pool{ limits, layouts };

    auto system = MakeCompiledSystem(4, 1, 1, 1, 1);
    system.ParticleStateLayout = (EParticleStateLayout)1;

    EXPECT_FALSE(pool.Allocate(system));
}

TEST(ResourcePoolTest, RollsBackOperandAndConstantCapacityFailures)
{
    for (const bool exhaustConstants : {false, true})
    {
        auto limits = MakePoolLimits();
        limits.OperandCapacity = exhaustConstants ? 4 : 2;
        limits.ConstantCapacity = 3;
        const ParticleStateLayoutRegistry layouts{ limits.ParticleCapacity };
        ResourcePool pool{ limits, layouts };
        EXPECT_FALSE(pool.Allocate(MakeCompiledSystem(4, 1, 2, 1, 1)));
        const auto allocation = pool.Allocate(MakeCompiledSystem(4, 1, 1, 1, 1));
        ASSERT_TRUE(allocation);
        EXPECT_EQ(allocation->InstanceIndex, 0u);
        EXPECT_EQ(allocation->Operations.Offset, 0u);
        EXPECT_EQ(allocation->Operands.Offset, 0u);
        EXPECT_EQ(allocation->Constants.Offset, 0u);
    }
}

TEST(ResourcePoolTest, SharesProgramsButKeepsInstanceParametersSeparate)
{
    auto limits = MakePoolLimits();
    limits.OperationCapacity = 1;
    limits.OperandCapacity = 2;
    limits.ConstantCapacity = 3;
    const ParticleStateLayoutRegistry layouts{limits.ParticleCapacity};
    ResourcePool pool{limits, layouts};
    const auto system = MakeCompiledSystem(4, 1, 1, 1, 1);
    const auto first = pool.Allocate(system);
    const auto second = pool.Allocate(system);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_TRUE(first->UploadProgram);
    EXPECT_FALSE(second->UploadProgram);
    EXPECT_EQ(first->ProgramAllocationId, second->ProgramAllocationId);
    EXPECT_EQ(first->Operands.Offset, second->Operands.Offset);
    EXPECT_EQ(first->Constants.Offset, second->Constants.Offset);
    EXPECT_NE(first->Parameters.Offset, second->Parameters.Offset);

    pool.Release(*first);
    EXPECT_FALSE(pool.Allocate(MakeCompiledSystem(4, 1, 1, 1, 1)));
    pool.Release(*second);
    const auto replacement = pool.Allocate(MakeCompiledSystem(4, 1, 1, 1, 1));
    ASSERT_TRUE(replacement);
    EXPECT_TRUE(replacement->UploadProgram);
    EXPECT_EQ(replacement->Operands.Offset, 0u);
    EXPECT_EQ(replacement->Constants.Offset, 0u);
}

TEST(ResourcePoolTest, KeepsProgramRevisionsAliveUntilTheirLastInstanceRetires)
{
    const auto limits = MakePoolLimits();
    const ParticleStateLayoutRegistry layouts{limits.ParticleCapacity};
    ResourcePool pool{limits, layouts};
    auto system = MakeCompiledSystem(4, 1, 1, 1, 1);
    system.CompilationRevision = 1;
    const auto oldRevision = pool.Allocate(system);
    ASSERT_TRUE(oldRevision);
    system.CompilationRevision = 2;
    const auto newRevision = pool.Allocate(system);
    ASSERT_TRUE(newRevision);
    EXPECT_NE(oldRevision->ProgramAllocationId, newRevision->ProgramAllocationId);
    EXPECT_NE(oldRevision->Operations.Offset, newRevision->Operations.Offset);
    EXPECT_NE(oldRevision->Operands.Offset, newRevision->Operands.Offset);
    pool.Release(*oldRevision);
    const auto another = pool.Allocate(system);
    ASSERT_TRUE(another);
    EXPECT_EQ(newRevision->ProgramAllocationId, another->ProgramAllocationId);
}

TEST(ResourcePoolTest, FailedSharedInstanceAllocationDoesNotReleaseTheLiveProgram)
{
    auto limits = MakePoolLimits();
    limits.OperationCapacity = 1;
    limits.ParticleCapacity = 4;
    const ParticleStateLayoutRegistry layouts{limits.ParticleCapacity};
    ResourcePool pool{limits, layouts};
    const auto system = MakeCompiledSystem(4, 1, 1, 1, 1);
    const auto live = pool.Allocate(system);
    ASSERT_TRUE(live);
    EXPECT_FALSE(pool.Allocate(system));
    pool.Release(*live);
    EXPECT_TRUE(pool.Allocate(MakeCompiledSystem(4, 1, 1, 1, 1)));
}
