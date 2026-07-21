#include <gtest/gtest.h>

#include <Engine/Aether/SystemInstance.h>

using namespace Elixir;
using namespace Elixir::Aether;

TEST(AetherSystemInstanceTest, ReplacesCompiledSystemAndIncrementsRevision)
{
    const auto initialSystem = CreateRef<SCompiledSystem>();
    const auto replacementSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ initialSystem };

    const auto initialRevision = instance.GetRevision();

    instance.SetCompiledSystem(replacementSystem);

    EXPECT_EQ(instance.GetRevision(), initialRevision + 1);
    EXPECT_EQ(&instance.GetCompiledSystem(), replacementSystem.get());
}

TEST(AetherSystemInstanceTest, DoesNotIncrementRevisionForSameCompiledSystem)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ compiledSystem };

    const auto initialRevision = instance.GetRevision();
    instance.SetCompiledSystem(compiledSystem);

    EXPECT_EQ(instance.GetRevision(), initialRevision);
}