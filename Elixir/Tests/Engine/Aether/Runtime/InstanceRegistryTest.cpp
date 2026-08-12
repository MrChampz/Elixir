#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include <Engine/Aether/Runtime/InstanceRegistry.h>

#include "../TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;
using namespace Elixir::Aether::Runtime;

namespace
{
    Ref<const SystemInstanceRenderProxy> Capture(
        const Ref<SystemInstance>& instance
    )
    {
        FrameSubmission submission;
        EXPECT_TRUE(submission.Submit(*instance));

        if (submission.IsEmpty())
            return nullptr;

        return submission.GetRenderProxies().front();
    }
}

TEST(InstanceRegistryTest, RetainsAuthoredSystemUntilFirstRegistration)
{
    TestInstanceRegistry runtime;
    std::weak_ptr<System> authoredSystem;
    Ref<SystemInstance> instance;

    {
        auto system = CreateRef<System>("Transient authored system");
        system->GetParameters().SetFloat4("Tint", { 1.0f, 0.5f, 0.25f, 1.0f });
        authoredSystem = system;
        instance = system->CreateInstance();

        ASSERT_TRUE(instance);
        EXPECT_EQ(instance->GetSourceSystemId(), system->GetId());

        system.reset();
        EXPECT_FALSE(authoredSystem.expired());
    }

    ASSERT_TRUE(runtime.Registry.Register(instance));
    EXPECT_TRUE(authoredSystem.expired());

    const auto tint = instance->GetParameterValue("Tint");
    ASSERT_TRUE(tint.has_value());
    EXPECT_EQ(*tint, glm::vec4(1.0f, 0.5f, 0.25f, 1.0f));
}

TEST(InstanceRegistryTest, ReusesCompiledDataForTheSameSystem)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Shared compiled system");
    const auto first = system->CreateInstance();
    const auto second = system->CreateInstance();

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(runtime.Registry.Register(first));
    ASSERT_TRUE(runtime.Registry.Register(second));

    const auto firstProxy = Capture(first);
    const auto secondProxy = Capture(second);

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

    const auto instance = system->CreateInstance();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->SetParameterOverride(
        "Tint",
        { 0.25f, 0.5f, 0.75f, 1.0f }
    ));
    ASSERT_TRUE(runtime.Registry.Register(instance));

    const auto before = Capture(instance);
    ASSERT_TRUE(before);

    system->GetParameters().SetFloat4("Tint", { 0.0f, 0.0f, 0.0f, 1.0f });
    ASSERT_TRUE(runtime.Registry.Recompile(system));

    const auto after = Capture(instance);
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
    const auto instance = runtime.CreateRegisteredInstance("Detached system");

    ASSERT_TRUE(instance);
    runtime.Registry.PublishActiveInstances();

    ASSERT_EQ(runtime.Registry.Unregister(instance), instance);

    const auto published = runtime.Registry.AcquireSubmission();
    ASSERT_TRUE(published);
    EXPECT_TRUE(published->IsEmpty());

    EXPECT_FALSE(runtime.Registry.Register(instance));
    EXPECT_FALSE(runtime.Registry.Unregister(instance));
}

TEST(InstanceRegistryTest, PublishesRegisteredInstanceInEveryFrame)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateRegisteredInstance("Persistent system");
    ASSERT_TRUE(instance);

    runtime.Registry.PublishActiveInstances();
    const auto firstFrame = runtime.Registry.AcquireSubmission();

    runtime.Registry.PublishActiveInstances();
    const auto secondFrame = runtime.Registry.AcquireSubmission();

    ASSERT_TRUE(firstFrame);
    ASSERT_TRUE(secondFrame);
    EXPECT_EQ(firstFrame->GetInstanceCount(), 1);
    EXPECT_EQ(secondFrame->GetInstanceCount(), 1);
    EXPECT_NE(firstFrame, secondFrame);
}

TEST(InstanceRegistryTest, RejectsDuplicateRegistration)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Duplicate submission");
    const auto instance = system->CreateInstance();

    EXPECT_TRUE(runtime.Registry.Register(instance));
    EXPECT_FALSE(runtime.Registry.Register(instance));
}

TEST(InstanceRegistryTest, AcceptsConcurrentRegistrations)
{
    constexpr size_t instanceCount = 64;

    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Concurrent submission");
    std::vector<Ref<SystemInstance>> instances;
    std::vector<std::thread> threads;
    std::atomic_size_t accepted = 0;

    instances.reserve(instanceCount);
    threads.reserve(instanceCount);

    for (size_t index = 0; index < instanceCount; ++index)
        instances.push_back(system->CreateInstance());

    for (const auto& instance : instances)
    {
        threads.emplace_back([&runtime, &accepted, instance]
        {
            if (runtime.Registry.Register(instance))
                accepted.fetch_add(1, std::memory_order_relaxed);
        });
    }

    for (auto& thread : threads)
        thread.join();

    runtime.Registry.PublishActiveInstances();
    const auto submission = runtime.Registry.AcquireSubmission();

    ASSERT_TRUE(submission);
    EXPECT_EQ(accepted.load(std::memory_order_relaxed), instanceCount);
    EXPECT_EQ(submission->GetInstanceCount(), instanceCount);
}
