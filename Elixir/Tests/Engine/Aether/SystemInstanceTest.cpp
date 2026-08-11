#include <gtest/gtest.h>

#include <barrier>
#include <concepts>
#include <thread>

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>

#include "TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;

template <typename T>
concept HasPublicSnapshotCapture = requires(const T& instance)
{
    instance.CaptureSnapshot();
};

static_assert(!HasPublicSnapshotCapture<SystemInstance>);

static_assert(
    !std::constructible_from<
        SystemInstance,
        Ref<const SCompiledSystem>
    >
);

namespace
{
    Ref<const SystemInstanceRenderProxy> CaptureForTest(
        const Ref<SystemInstance>& instance
    )
    {
        FrameSubmission submission;
        EXPECT_TRUE(submission.Submit(*instance));
        return submission.GetRenderProxies().front();
    }

    Ref<System> MakeSystem()
    {
        const auto system = CreateRef<System>("System instance test");
        system->GetParameters().SetFloat4("Tint", { 1.0f, 1.0f, 1.0f, 1.0f });
        system->GetCurves().SetCurve("SizeOverLife", { 0.0f, 0.5f, 1.0f });

        return system;
    }
}

TEST(SystemInstanceTest, RecompilesSystemAndIncrementsRevision)
{
    TestInstanceRegistry runtime;
    const auto system = MakeSystem();
    const auto instance = runtime.Registry.CreateInstance(system);
    ASSERT_TRUE(instance);

    const auto initialRevision = CaptureForTest(instance)->GetRevision();
    system->GetParameters().SetFloat4("Tint", { 0.5f, 0.5f, 0.5f, 1.0f });

    ASSERT_TRUE(runtime.Registry.Recompile(system));

    const auto snapshot = CaptureForTest(instance);

    EXPECT_EQ(snapshot->GetRevision(), initialRevision + 1);
    EXPECT_EQ(snapshot->GetCompiledSystem().CompilationRevision, 2u);
}

TEST(SystemInstanceTest, ReturnsOverrideOrCompiledDefaultForExposedParameter)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.Registry.CreateInstance(MakeSystem());
    ASSERT_TRUE(instance);

    const auto defaultValue = instance->GetParameterValue("Tint");

    ASSERT_TRUE(defaultValue.has_value());
    EXPECT_FLOAT_EQ(defaultValue->x, 1.0f);
    EXPECT_FLOAT_EQ(defaultValue->y, 1.0f);
    EXPECT_FLOAT_EQ(defaultValue->z, 1.0f);
    EXPECT_FLOAT_EQ(defaultValue->w, 1.0f);

    ASSERT_TRUE(instance->SetParameterOverride(
        "Tint",
        { 0.25f, 0.5f, 0.75f, 1.0f }
    ));

    const auto overrideValue = instance->GetParameterValue("Tint");

    ASSERT_TRUE(overrideValue.has_value());
    EXPECT_FLOAT_EQ(overrideValue->x, 0.25f);
    EXPECT_FLOAT_EQ(overrideValue->y, 0.5f);
    EXPECT_FLOAT_EQ(overrideValue->z, 0.75f);
    EXPECT_FLOAT_EQ(overrideValue->w, 1.0f);
    EXPECT_FALSE(instance->GetParameterValue("SizeOverLife:0").has_value());
    EXPECT_FALSE(instance->GetParameterValue("Missing").has_value());
}

TEST(SystemInstanceTest, AppliesOverridesOnlyToExposedParameters)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.Registry.CreateInstance(MakeSystem());
    ASSERT_TRUE(instance);

    const auto initialParameterRevision = CaptureForTest(instance)->GetParameterRevision();

    EXPECT_TRUE(instance->SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));
    EXPECT_FALSE(instance->SetParameterOverride("SizeOverLife:0", { 1.0f, 1.0f, 1.0f, 1.0f }));

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
    TestInstanceRegistry runtime;
    const auto instance = runtime.Registry.CreateInstance(MakeSystem());
    ASSERT_TRUE(instance);

    ASSERT_TRUE(instance->SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));
    ASSERT_TRUE(instance->ClearParameterOverride("Tint"));
    EXPECT_FALSE(instance->ClearParameterOverride("Tint"));

    const auto tint = CaptureForTest(instance)->GetParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 1.0f);
    EXPECT_FLOAT_EQ(tint.y, 1.0f);
    EXPECT_FLOAT_EQ(tint.z, 1.0f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);
}

TEST(SystemInstanceTest, RetainsCompatibleOverridesAfterRecompilation)
{
    TestInstanceRegistry runtime;
    const auto system = MakeSystem();
    const auto instance = runtime.Registry.CreateInstance(system);
    ASSERT_TRUE(instance);

    ASSERT_TRUE(instance->SetParameterOverride("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    system->GetParameters().SetFloat4("Tint", { 0.0f, 0.0f, 0.0f, 1.0f });
    ASSERT_TRUE(runtime.Registry.Recompile(system));

    const auto tint = CaptureForTest(instance)->GetParameterValue(0);
    EXPECT_FLOAT_EQ(tint.x, 0.25f);
    EXPECT_FLOAT_EQ(tint.y, 0.5f);
    EXPECT_FLOAT_EQ(tint.z, 0.75f);
    EXPECT_FLOAT_EQ(tint.w, 1.0f);
}

TEST(SystemInstanceTest, StoresWorldTransformWithoutChangingCompiledSystem)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.Registry.CreateInstance(MakeSystem());
    ASSERT_TRUE(instance);
    const auto* compiledSystem = &CaptureForTest(instance)->GetCompiledSystem();

    glm::mat4 transform{ 1.0f };
    transform[3] = { 5.0f, 2.0f, -3.0f, 1.0f };

    instance->SetWorldTransform(transform);

    const auto snapshot = CaptureForTest(instance);

    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].x, 5.0f);
    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].y, 2.0f);
    EXPECT_FLOAT_EQ(snapshot->GetWorldTransform()[3].z, -3.0f);
    EXPECT_EQ(&snapshot->GetCompiledSystem(), compiledSystem);
}

TEST(SystemInstanceTest, KeepsCapturedProxyImmutableDuringConcurrentOverrides)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.Registry.CreateInstance(MakeSystem());
    ASSERT_TRUE(instance);
    const auto capturedBeforeOverrides = CaptureForTest(instance);
    std::barrier beginUpdates{ 2 };

    std::thread overrideThread([&]
    {
        beginUpdates.arrive_and_wait();

        for (uint32_t value = 2; value <= 64; ++value)
            instance->SetParameterOverride("Tint", { (float)value, 0.0f, 0.0f, 1.0f });
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
