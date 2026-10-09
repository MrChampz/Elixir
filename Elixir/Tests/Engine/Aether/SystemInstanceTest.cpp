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
    const auto instance = runtime.CreateRegisteredInstance(system);
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
    const auto instance = runtime.CreateRegisteredInstance(MakeSystem());
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
    const auto instance = runtime.CreateRegisteredInstance(MakeSystem());
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
    const auto instance = runtime.CreateRegisteredInstance(MakeSystem());
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
    const auto instance = runtime.CreateRegisteredInstance(system);
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
    const auto instance = runtime.CreateRegisteredInstance(MakeSystem());
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
    const auto instance = runtime.CreateRegisteredInstance(MakeSystem());
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

TEST(SystemInstanceTest, AppliesTypedOverridesBeforeAndAfterRegistration)
{
    using namespace Elixir::Aether::Core;
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Typed parameters");
    system->GetParameters().SetFloat("Size", 1.0f);
    system->GetParameters().SetFloat2("Pair", { 1, 2 });
    system->GetParameters().SetFloat3("Direction", { 3, 4, 5 });
    system->GetParameters().SetUInt("Ribbon", 0x80000001u);
    const auto instance = system->CreateInstance();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->SetParameterOverride("Size", 2.0f));
    ASSERT_TRUE(instance->SetParameterOverride("Pair", glm::vec2{ 6, 7 }));
    ASSERT_TRUE(instance->SetParameterOverride("Ribbon", UINT32_MAX));
    EXPECT_EQ(instance->GetParameterValue("Pair"), glm::vec4(6, 7, 0, 0));
    EXPECT_EQ(instance->GetParameterUInt("Ribbon"), UINT32_MAX);
    ASSERT_TRUE(runtime.Registry.Register(instance));
    const auto before = CaptureForTest(instance);
    const auto& compiled = before->GetCompiledSystem();
    const uint32_t ribbon = FindParameterIndex(compiled.Parameters, "Ribbon");
    EXPECT_EQ(before->GetParameterValue(FindParameterIndex(compiled.Parameters, "Size")), glm::vec4(2, 0, 0, 0));
    ASSERT_TRUE(instance->SetParameterOverride("Size", 3.0f));
    EXPECT_EQ(instance->GetParameterValue("Size"), glm::vec4(3, 0, 0, 0));
    EXPECT_EQ(before->GetParameterUInt(ribbon), UINT32_MAX);
    ASSERT_TRUE(instance->SetParameterOverride("Direction", glm::vec3{ 8, 9, 10 }));
    ASSERT_TRUE(instance->SetParameterOverride("Ribbon", 0x80000001u));
    EXPECT_EQ(instance->GetParameterValue("Direction"), glm::vec4(8, 9, 10, 0));
    EXPECT_EQ(instance->GetParameterUInt("Ribbon"), 0x80000001u);
    EXPECT_EQ(before->GetParameterUInt(ribbon), UINT32_MAX);
    EXPECT_EQ(CaptureForTest(instance)->GetParameterUInt(ribbon), 0x80000001u);
    EXPECT_FALSE(instance->SetParameterOverride("Ribbon", glm::vec4(1)));
    EXPECT_FALSE(instance->SetParameterOverride("Pair", 1u));
    EXPECT_FALSE(instance->SetParameterOverride("Pair", 1.0f));
    EXPECT_FALSE(instance->SetParameterOverride("Ribbon", 1.0f));
    EXPECT_FALSE(instance->SetParameterOverride("Missing", 1.0f));
    EXPECT_FALSE(instance->SetParameterOverride("Direction", glm::vec2{ 1, 2 }));
    EXPECT_FALSE(instance->SetParameterOverride("Pair", glm::vec3{ 1, 2, 3 }));
    EXPECT_FALSE(instance->GetParameterValue("Ribbon"));
    EXPECT_FALSE(instance->GetParameterUInt("Pair"));
    EXPECT_FALSE(instance->GetParameterUInt("Missing"));
    ASSERT_TRUE(instance->ClearParameterOverride("Ribbon"));
    EXPECT_EQ(instance->GetParameterUInt("Ribbon"), 0x80000001u);
}

TEST(SystemInstanceTest, DiscardsOverridesWhenTheDeclaredTypeChanges)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Changing parameter types");
    system->GetParameters().SetUInt("Value", 17);
    const auto instance = runtime.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->SetParameterOverride("Value", UINT32_MAX));
    system->GetParameters().SetFloat3("Value", { 1, 2, 3 });
    ASSERT_TRUE(runtime.Registry.Recompile(system));
    EXPECT_FALSE(instance->GetParameterUInt("Value"));
    EXPECT_EQ(instance->GetParameterValue("Value"), glm::vec4(1, 2, 3, 0));
    ASSERT_TRUE(instance->SetParameterOverride("Value", glm::vec3{ 4, 5, 6 }));
    EXPECT_EQ(instance->GetParameterValue("Value"), glm::vec4(4, 5, 6, 0));
}

TEST(SystemInstanceTest, RebindsOverridesWhenNativeParameterOffsetsChange)
{
    using namespace Elixir::Aether::Core;
    TestInstanceRegistry runtime;
    const auto system = MakeSystem();
    const auto instance = runtime.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->SetParameterOverride("Tint", { 2, 3, 4, 5 }));
    const auto before = CaptureForTest(instance);
    const auto previousIndex = FindParameterIndex(before->GetCompiledSystem().Parameters, "Tint");
    system->GetParameters().SetFloat("Size", 7);
    ASSERT_TRUE(runtime.Registry.Recompile(system));
    const auto after = CaptureForTest(instance);
    const auto& compiled = after->GetCompiledSystem();
    const auto currentIndex = FindParameterIndex(compiled.Parameters, "Tint");
    ASSERT_NE(currentIndex, previousIndex);
    EXPECT_EQ(compiled.Parameters[currentIndex].WordOffset, 1u);
    EXPECT_EQ(after->GetParameterValue(currentIndex), glm::vec4(2, 3, 4, 5));
    EXPECT_EQ(before->GetParameterValue(previousIndex), glm::vec4(2, 3, 4, 5));
    EXPECT_FLOAT_EQ(after->GetParameterValue(FindParameterIndex(compiled.Parameters, "Size")).x, 7);
    EXPECT_EQ(after->GetParameterWords().size(), before->GetParameterWords().size() + 1);
}
