#include <gtest/gtest.h>

#include <Engine/Aether/System.h>

using namespace Elixir;
using namespace Elixir::Aether;

TEST(AetherSystemTest, BuildAssignsContiguousEmitterParticleOffsets)
{
    System system{ "Particle offset contract" };
    system.AddEmitter("First", 3u, 0.0f);
    system.AddEmitter("Second", 7u, 0.0f);
    system.AddEmitter("Third", 11u, 0.0f);

    const auto compiled = system.Build();

    ASSERT_EQ(compiled.Emitters.size(), 3u);
    EXPECT_EQ(compiled.Emitters[0].ParticleOffset, 0u);
    EXPECT_EQ(compiled.Emitters[1].ParticleOffset, 3u);
    EXPECT_EQ(compiled.Emitters[2].ParticleOffset, 10u);
    EXPECT_EQ(compiled.TotalMaxParticles, 21u);
}

TEST(AetherSystemTest, BuildResolvesTriggerEmitterByCompiledIndex)
{
    System system{ "Trigger contract" };
    system.AddEmitter("Source", 8u, 0.0f);

    auto& target = system.AddEmitter("Target", 8u, 0.0f);
    target.SetTriggerEmitter("Source", 0.25f);

    const auto compiled = system.Build();

    ASSERT_EQ(compiled.Emitters.size(), 2u);
    EXPECT_EQ(compiled.Emitters[1].TriggerSourceEmitterIndex, 0);
    EXPECT_FLOAT_EQ(compiled.Emitters[1].TriggerDelaySeconds, 0.25f);
}