#include <gtest/gtest.h>

#include <concepts>
#include <utility>

#include <Engine/Aether/Rendering/FrameSubmission.h>

#include "../TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;

template <typename T>
concept HasPublicSystemInstanceId = requires(const T& instance)
{
    instance.GetId();
};

static_assert(!HasPublicSystemInstanceId<SystemInstance>);

template <typename T>
concept HasFrameSnapshots = requires(const T& submission)
{
    submission.GetSnapshots();
};

static_assert(!HasFrameSnapshots<FrameSubmission>);

static_assert(
    std::same_as<
        decltype(std::declval<const FrameSubmission&>().GetRenderProxies()),
        const std::vector<Ref<const SystemInstanceRenderProxy>>&
    >
);

TEST(FrameSubmissionTest, RetainsEachSystemInstanceAtMostOnce)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Frame submission system");
    const auto firstInstance = runtime.CreateRegisteredInstance(system);
    const auto secondInstance = runtime.CreateRegisteredInstance(system);

    ASSERT_TRUE(firstInstance);
    ASSERT_TRUE(secondInstance);

    FrameSubmission submission;

    EXPECT_TRUE(submission.Submit(*firstInstance));
    EXPECT_FALSE(submission.Submit(*firstInstance));
    EXPECT_TRUE(submission.Submit(*secondInstance));

    ASSERT_EQ(submission.GetInstanceCount(), 2);
    EXPECT_NE(submission.GetRenderProxies()[0], submission.GetRenderProxies()[1]);
}

TEST(FrameSubmissionTest, ResetKeepsTheSubmissionReusable)
{
    TestInstanceRegistry runtime;
    const auto system = CreateRef<System>("Reusable submission system");
    const auto firstInstance = runtime.CreateRegisteredInstance(system);
    const auto secondInstance = runtime.CreateRegisteredInstance(system);

    ASSERT_TRUE(firstInstance);
    ASSERT_TRUE(secondInstance);

    FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*firstInstance));

    submission.Reset();

    EXPECT_TRUE(submission.IsEmpty());
    EXPECT_EQ(submission.GetInstanceCount(), 0);
    EXPECT_TRUE(submission.Submit(*firstInstance));
    EXPECT_TRUE(submission.Submit(*secondInstance));
    EXPECT_EQ(submission.GetInstanceCount(), 2);
}

TEST(FrameSubmissionTest, RemovesAnInstanceBeforeItIsRetired)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateRegisteredInstance("Removed submission system");
    ASSERT_TRUE(instance);

    FrameSubmission submission;

    ASSERT_TRUE(submission.Submit(*instance));
    EXPECT_TRUE(submission.Remove(*instance));
    EXPECT_EQ(submission.GetInstanceCount(), 0);
    EXPECT_FALSE(submission.Remove(*instance));
    EXPECT_TRUE(submission.Submit(*instance));
}

TEST(FrameSubmissionTest, RetainsTheStateCapturedAtSubmission)
{
    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateRegisteredInstance("Captured submission system");
    ASSERT_TRUE(instance);

    FrameSubmission submission;

    ASSERT_TRUE(submission.Submit(*instance));

    glm::mat4 transform{ 1.0f };
    transform[3] = { 3.0f, 2.0f, 1.0f, 1.0f };
    instance->SetWorldTransform(transform);

    const auto& proxy = submission.GetRenderProxies().front();
    EXPECT_FLOAT_EQ(proxy->GetWorldTransform()[3].x, 0.0f);
    EXPECT_FLOAT_EQ(proxy->GetWorldTransform()[3].y, 0.0f);
    EXPECT_FLOAT_EQ(proxy->GetWorldTransform()[3].z, 0.0f);
}
