#include <gtest/gtest.h>

#include <barrier>
#include <thread>

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;

template <typename T>
concept HasPublicSnapshotCapture = requires(const T& instance)
{
    instance.CaptureSnapshot();
};

static_assert(!HasPublicSnapshotCapture<SystemInstance>);

namespace
{
    Ref<const SystemInstanceRenderProxy> CaptureForTest(const SystemInstance& instance)
    {
        FrameSubmission submission;
        EXPECT_TRUE(submission.Submit(instance));
        return submission.GetRenderProxies().front();
    }

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

TEST(SystemInstanceTest, ReplacesCompiledSystemAndIncrementsRevision)
{
    const auto initialSystem = CreateRef<SCompiledSystem>();
    const auto replacementSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ initialSystem };

    const auto initialRevision = CaptureForTest(instance)->GetRevision();

    instance.SetCompiledSystem(replacementSystem);

    const auto snapshot = CaptureForTest(instance);

    EXPECT_EQ(snapshot->GetRevision(), initialRevision + 1);
    EXPECT_EQ(&snapshot->GetCompiledSystem(), replacementSystem.get());
}

TEST(SystemInstanceTest, DoesNotIncrementRevisionForSameCompiledSystem)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ compiledSystem };
    instance.SetCompiledSystem(compiledSystem);

    EXPECT_EQ(CaptureForTest(instance)->GetRevision(), 1);
}

TEST(SystemInstanceTest, ReturnsOverrideOrCompiledDefaultForExposedParameter)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    const auto defaultValue = instance.GetParameterValue("Tint");

    ASSERT_TRUE(defaultValue.has_value());
    EXPECT_FLOAT_EQ(defaultValue->x, 1.0f);
    EXPECT_FLOAT_EQ(defaultValue->y, 1.0f);
    EXPECT_FLOAT_EQ(defaultValue->z, 1.0f);
    EXPECT_FLOAT_EQ(defaultValue->w, 1.0f);

    ASSERT_TRUE(instance.SetParameterOverride(
        "Tint",
        { 0.25f, 0.5f, 0.75f, 1.0f }
    ));

    const auto overrideValue = instance.GetParameterValue("Tint");

    ASSERT_TRUE(overrideValue.has_value());
    EXPECT_FLOAT_EQ(overrideValue->x, 0.25f);
    EXPECT_FLOAT_EQ(overrideValue->y, 0.5f);
    EXPECT_FLOAT_EQ(overrideValue->z, 0.75f);
    EXPECT_FLOAT_EQ(overrideValue->w, 1.0f);
    EXPECT_FALSE(instance.GetParameterValue("SizeOverLife:0").has_value());
    EXPECT_FALSE(instance.GetParameterValue("Missing").has_value());
}

TEST(SystemInstanceTest, AppliesOverridesOnlyToExposedParameters)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    const auto initialParameterRevision = CaptureForTest(instance)->GetParameterRevision();

    EXPECT_TRUE(instance.SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));
    EXPECT_FALSE(instance.SetParameterOverride("SizeOverLife:0", { 1.0f, 1.0f, 1.0f, 1.0f }));

    const auto snapshot = CaptureForTest(instance);
    EXPECT_EQ(snapshot->GetParameterRevision(), initialParameterRevision + 1);

    const auto tint = snapshot->GetParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 0.25f);
    EXPECT_FLOAT_EQ(tint.y, 0.5f);
    EXPECT_FLOAT_EQ(tint.z, 0.75f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);

    const auto colorChunk = snapshot->GetParameterValue(1);
    EXPECT_FLOAT_EQ(colorChunk.x, 0.0f);
    EXPECT_FLOAT_EQ(colorChunk.y, 0.5f);
    EXPECT_FLOAT_EQ(colorChunk.z, 1.0f);
    EXPECT_FLOAT_EQ(colorChunk.w, 1.0f);
}

TEST(SystemInstanceTest, ClearsOverridesAndRestoresCompiledDefaults)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    ASSERT_TRUE(instance.SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));
    ASSERT_TRUE(instance.ClearParameterOverride("Tint"));
    EXPECT_FALSE(instance.ClearParameterOverride("Tint"));

    const auto tint = CaptureForTest(instance)->GetParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 1.0f);
    EXPECT_FLOAT_EQ(tint.y, 1.0f);
    EXPECT_FLOAT_EQ(tint.z, 1.0f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);
}

TEST(SystemInstanceTest, RetainsOnlyOverridesExposedByReplacementSystem)
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

    const auto tint = CaptureForTest(instance)->GetParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 0.25f);
    EXPECT_FLOAT_EQ(tint.y, 0.5f);
    EXPECT_FLOAT_EQ(tint.z, 0.75f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);
}

TEST(SystemInstanceTest, StoresWorldTransformWithoutChangingCompiledSystem)
{
    const auto compiledSystem = MakeCompiledSystem();
    SystemInstance instance{ compiledSystem };

    glm::mat4 transform{ 1.0f };
    transform[3] = { 5.0f, 2.0f, -3.0f, 1.0f };

    instance.SetWorldTransform(transform);

    const auto snapshot = CaptureForTest(instance);

    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].x, 5.0f);
    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].y, 2.0f);
    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].z, -3.0f);
    EXPECT_EQ(&snapshot->GetCompiledSystem(), compiledSystem.get());
}

TEST(SystemInstanceTest, KeepsCapturedProxyImmutableDuringConcurrentOverrides)
{
    const auto system = MakeCompiledSystem();
    SystemInstance instance{ system };
    const auto capturedBeforeOverrides = CaptureForTest(instance);
    std::barrier beginUpdates{ 2 };

    std::thread overrideThread([&]
    {
        beginUpdates.arrive_and_wait();

        for (uint32_t value = 2; value <= 64; ++value)
            instance.SetParameterOverride("Tint", { (float)value, 0.0f, 0.0f, 1.0f });
    });

    beginUpdates.arrive_and_wait();

    for (uint32_t capture = 0; capture < 64; ++capture)
    {
        const auto proxy = CaptureForTest(instance);
        EXPECT_GE(proxy->GetParameterValue(0).x, 1.0f);
        EXPECT_LE(proxy->GetParameterValue(0).x, 64.0f);
    }

    overrideThread.join();

    EXPECT_FLOAT_EQ(capturedBeforeOverrides->GetParameterValue(0).x, 1.0f);
    EXPECT_FLOAT_EQ(CaptureForTest(instance)->GetParameterValue(0).x, 64.0f);
}
