#include <gtest/gtest.h>

#include <Engine/Aether/System.h>

using namespace Elixir;
using namespace Elixir::Aether;

TEST(AetherSystemTest, CompileAssignsContiguousLocalEmitterParticleOffsets)
{
    System system{ "Particle offset contract" };
    system.AddEmitter("First", 3u, 0.0f);
    system.AddEmitter("Second", 7u, 0.0f);
    system.AddEmitter("Third", 11u, 0.0f);

    const auto compiled = system.Compile();

    ASSERT_EQ(compiled.Emitters.size(), 3u);
    EXPECT_EQ(compiled.ParticleStateLayout, EParticleStateLayout::CoreV1);
    EXPECT_EQ(compiled.Emitters[0].LocalParticleOffset, 0u);
    EXPECT_EQ(compiled.Emitters[1].LocalParticleOffset, 3u);
    EXPECT_EQ(compiled.Emitters[2].LocalParticleOffset, 10u);
    EXPECT_EQ(compiled.TotalMaxParticles, 21u);
}

TEST(AetherSystemTest, CompileResolvesTriggerEmitterByCompiledIndex)
{
    System system{ "Trigger contract" };
    system.AddEmitter("Source", 8u, 0.0f);

    auto& target = system.AddEmitter("Target", 8u, 0.0f);
    target.SetTriggerEmitter("Source", 0.25f);

    const auto compiled = system.Compile();

    ASSERT_EQ(compiled.Emitters.size(), 2u);
    EXPECT_EQ(compiled.Emitters[1].TriggerSourceEmitterIndex, 0);
    EXPECT_FLOAT_EQ(compiled.Emitters[1].TriggerDelaySeconds, 0.25f);
}