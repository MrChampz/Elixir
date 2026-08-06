#include <gtest/gtest.h>

#include <Engine/Aether/SystemInstance.h>

using namespace Elixir;
using namespace Elixir::Aether;

namespace
{
    Ref<SCompiledSystem> MakeCompiledSystem()
    {
        const auto system = CreateRef<SCompiledSystem>();

        system->Parameters = {
            { "Tint", { 1.0f, 1.0f, 1.0f, 1.0f } },
            { "SizeOverLife:0", { 0.0f, 0.5f, 1.0f, 1.0f } },
        };

        system->ExposedParameters = {
            { "Tint", 0u },
        };

        return system;
    }
}

TEST(AetherSystemInstanceTest, ReplacesCompiledSystemAndIncrementsRevision)
{
    const auto initialSystem = CreateRef<SCompiledSystem>();
    const auto replacementSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ initialSystem };

    const auto initialRevision = instance.CaptureSnapshot()->GetRevision();

    instance.SetCompiledSystem(replacementSystem);

    const auto snapshot = instance.CaptureSnapshot();

    EXPECT_EQ(snapshot->GetRevision(), initialRevision + 1);
    EXPECT_EQ(&snapshot->GetCompiledSystem(), replacementSystem.get());
}

TEST(AetherSystemInstanceTest, DoesNotIncrementRevisionForSameCompiledSystem)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ compiledSystem };
    instance.SetCompiledSystem(compiledSystem);

    EXPECT_EQ(instance.CaptureSnapshot()->GetRevision(), 1);
}

TEST(AetherSystemInstanceTest, AppliesOverridesOnlyToExposedParameters)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    const auto initialParameterRevision = instance.CaptureSnapshot()->GetParameterRevision();

    EXPECT_TRUE(instance.SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));
    EXPECT_FALSE(instance.SetParameterOverride("SizeOverLife:0", { 1.0f, 1.0f, 1.0f, 1.0f }));

    const auto snapshot = instance.CaptureSnapshot();
    EXPECT_EQ(snapshot->GetParameterRevision(), initialParameterRevision + 1);

    const auto tint = snapshot->ResolveParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 0.25f);
    EXPECT_FLOAT_EQ(tint.y, 0.5f);
    EXPECT_FLOAT_EQ(tint.z, 0.75f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);

    const auto colorChunk = snapshot->ResolveParameterValue(1);
    EXPECT_FLOAT_EQ(colorChunk.x, 0.0f);
    EXPECT_FLOAT_EQ(colorChunk.y, 0.5f);
    EXPECT_FLOAT_EQ(colorChunk.z, 1.0f);
    EXPECT_FLOAT_EQ(colorChunk.w, 1.0f);
}

TEST(AetherSystemInstanceTest, ClearsOverridesAndRestoresCompiledDefaults)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    ASSERT_TRUE(instance.SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));
    ASSERT_TRUE(instance.ClearParameterOverride("Tint"));
    EXPECT_FALSE(instance.ClearParameterOverride("Tint"));

    const auto tint = instance.CaptureSnapshot()->ResolveParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 1.0f);
    EXPECT_FLOAT_EQ(tint.y, 1.0f);
    EXPECT_FLOAT_EQ(tint.z, 1.0f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);
}

TEST(AetherSystemInstanceTest, RetainsOnlyOverridesExposedByReplacementSystem)
{
    const auto initialSystem = MakeCompiledSystem();
    SystemInstance instance{ initialSystem };

    ASSERT_TRUE(instance.SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    const auto replacementSystem = CreateRef<SCompiledSystem>();
    replacementSystem->Parameters = {
        { "Tint", { 1.0f, 1.0f, 1.0f, 1.0f } },
    };
    replacementSystem->ExposedParameters = {
        { "Tint", 0u },
    };

    instance.SetCompiledSystem(replacementSystem);

    const auto tint = instance.CaptureSnapshot()->ResolveParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 0.25f);
    EXPECT_FLOAT_EQ(tint.y, 0.5f);
    EXPECT_FLOAT_EQ(tint.z, 0.75f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);
}

TEST(AetherSystemInstanceTest, StoresWorldTransformWithoutChangingCompiledSystem)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    glm::mat4 transform{ 1.0f };
    transform[3] = { 5.0f, 2.0f, -3.0f, 1.0f };

    instance.SetWorldTransform(transform);

    const auto snapshot = instance.CaptureSnapshot();

    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].x, 5.0f);
    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].y, 2.0f);
    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].z, -3.0f);
    EXPECT_EQ(&snapshot->GetCompiledSystem(), compiledSystem.get());
}
