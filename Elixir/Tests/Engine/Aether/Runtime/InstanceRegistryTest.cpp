#include <gtest/gtest.h>

#include <Engine/Aether/Runtime/InstanceRegistry.h>

#include "../TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;
using namespace Elixir::Aether::Runtime;

namespace
{
    Ref<const SystemInstanceRenderProxy> Capture(
        InstanceRegistry& registry,
        const Ref<SystemInstance>& instance
    )
    {
        FrameSubmission submission;
        EXPECT_TRUE(registry.Submit(submission, instance));

        if (submission.IsEmpty())
            return nullptr;

        return submission.GetRenderProxies().front();
    }
}

TEST(InstanceRegistryTest, CreatesAnInstanceWithoutRetainingItsSystem)
{
    TestInstanceRegistry runtime;
    std::weak_ptr<System> authoredSystem;
    Ref<SystemInstance> instance;

    {
        const auto system = CreateRef<System>("Transient authored system");
        system->GetParameters().SetFloat4("Tint", { 1.0f, 0.5f, 0.25f, 1.0f });
        authoredSystem = system;
        instance = runtime.Registry.CreateInstance(system);

        ASSERT_TRUE(instance);
        EXPECT_EQ(instance->GetSourceSystemId(), system->GetId());
    }

    EXPECT_TRUE(authoredSystem.expired());

    const auto tint = instance->GetParameterValue("Tint");
    ASSERT_TRUE(tint.has_value());
    EXPECT_EQ(*tint, glm::vec4(1.0f, 0.5f, 0.25f, 1.0f));
}

TEST(InstanceRegistryTest, ReusesCompiledDataForTheSameSystem)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Shared compiled system");
    const auto first = runtime.Registry.CreateInstance(system);
    const auto second = runtime.Registry.CreateInstance(system);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    const auto firstProxy = Capture(runtime.Registry, first);
    const auto secondProxy = Capture(runtime.Registry, second);

    ASSERT_TRUE(firstProxy);
    ASSERT_TRUE(secondProxy);
    EXPECT_EQ(
        &firstProxy->GetCompiledSystem(),
        &secondProxy->GetCompiledSystem()
    );
    EXPECT_EQ(firstProxy->GetCompiledSystem().CompilationRevision, 1u);
}

TEST(InstanceRegistryTest, RecompilesExistingInstancesAndRetainsOverrides)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Recompiled system");
    system->GetParameters().SetFloat4("Tint", { 1.0f, 1.0f, 1.0f, 1.0f });

    const auto instance = runtime.Registry.CreateInstance(system);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->SetParameterOverride(
        "Tint",
        { 0.25f, 0.5f, 0.75f, 1.0f }
    ));

    const auto before = Capture(runtime.Registry, instance);
    ASSERT_TRUE(before);

    system->GetParameters().SetFloat4("Tint", { 0.0f, 0.0f, 0.0f, 1.0f });
    ASSERT_TRUE(runtime.Registry.Recompile(system));

    const auto after = Capture(runtime.Registry, instance);
    ASSERT_TRUE(after);
    EXPECT_EQ(after->GetRevision(), before->GetRevision() + 1);
    EXPECT_EQ(
        after->GetCompiledSystem().CompilationRevision,
        before->GetCompiledSystem().CompilationRevision + 1
    );
    EXPECT_NE(&after->GetCompiledSystem(), &before->GetCompiledSystem());
    EXPECT_EQ(
        after->GetParameterValue(0),
        glm::vec4(0.25f, 0.5f, 0.75f, 1.0f)
    );
}

TEST(InstanceRegistryTest, DetachesAnInstanceFromPublishedFrames)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateInstance("Detached system");
    const auto submission = CreateRef<FrameSubmission>();

    ASSERT_TRUE(instance);
    ASSERT_TRUE(runtime.Registry.Submit(*submission, instance));
    runtime.Registry.Publish(submission);

    ASSERT_EQ(runtime.Registry.DetachInstance(instance), instance);

    const auto published = runtime.Registry.AcquireSubmission();
    ASSERT_TRUE(published);
    EXPECT_TRUE(published->IsEmpty());

    FrameSubmission replacement;
    EXPECT_FALSE(runtime.Registry.Submit(replacement, instance));
    EXPECT_FALSE(runtime.Registry.DetachInstance(instance));
}
