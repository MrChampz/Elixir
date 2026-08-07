#include <gtest/gtest.h>

#include <Engine/Aether/FrameSubmission.h>

using namespace Elixir;
using namespace Elixir::Aether;

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

TEST(AetherFrameSubmissionTest, RetainsEachSystemInstanceAtMostOnce)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    const SystemInstance firstInstance{ compiledSystem };
    const SystemInstance secondInstance{ compiledSystem };

    FrameSubmission submission;

    EXPECT_TRUE(submission.Submit(firstInstance));
    EXPECT_FALSE(submission.Submit(firstInstance));
    EXPECT_TRUE(submission.Submit(secondInstance));

    ASSERT_EQ(submission.GetInstanceCount(), 2);
    EXPECT_NE(submission.GetRenderProxies()[0], submission.GetRenderProxies()[1]);
}

TEST(AetherFrameSubmissionTest, ResetKeepsTheSubmissionReusable)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    const SystemInstance firstInstance{ compiledSystem };
    const SystemInstance secondInstance{ compiledSystem };

    FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(firstInstance));

    submission.Reset();

    EXPECT_TRUE(submission.IsEmpty());
    EXPECT_EQ(submission.GetInstanceCount(), 0);
    EXPECT_TRUE(submission.Submit(firstInstance));
    EXPECT_TRUE(submission.Submit(secondInstance));
    EXPECT_EQ(submission.GetInstanceCount(), 2);
}

TEST(AetherFrameSubmissionTest, RemovesAnInstanceBeforeItIsRetired)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    const SystemInstance instance{ compiledSystem };
    FrameSubmission submission;

    ASSERT_TRUE(submission.Submit(instance));
    EXPECT_TRUE(submission.Remove(instance));
    EXPECT_EQ(submission.GetInstanceCount(), 0);
    EXPECT_FALSE(submission.Remove(instance));
    EXPECT_TRUE(submission.Submit(instance));
}

TEST(AetherFrameSubmissionTest, RetainsTheStateCapturedAtSubmission)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    SystemInstance instance{ compiledSystem };
    FrameSubmission submission;

    ASSERT_TRUE(submission.Submit(instance));

    glm::mat4 transform{ 1.0f };
    transform[3] = { 3.0f, 2.0f, 1.0f, 1.0f };
    instance.SetWorldTransform(transform);

    const auto& proxy = submission.GetRenderProxies().front();
    EXPECT_FLOAT_EQ(proxy->GetWorldTransform()[3].x, 0.0f);
    EXPECT_FLOAT_EQ(proxy->GetWorldTransform()[3].y, 0.0f);
    EXPECT_FLOAT_EQ(proxy->GetWorldTransform()[3].z, 0.0f);
}

TEST(AetherFrameSubmissionPublisherTest, PublishesOnlySealedSubmissions)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    const SystemInstance instance{ compiledSystem };
    const auto submission = CreateRef<FrameSubmission>();
    FrameSubmissionPublisher publisher;

    ASSERT_TRUE(submission->Submit(instance));
    publisher.Publish(submission);

    const auto published = publisher.Acquire();
    ASSERT_TRUE(published);
    EXPECT_TRUE(published->IsSealed());
    EXPECT_FALSE(submission->Submit(instance));
}

TEST(AetherFrameSubmissionPublisherTest, RemovesDestroyedInstanceFromPublishedFrame)
{
    const auto compiledSystem = CreateRef<SCompiledSystem>();
    const SystemInstance instance{ compiledSystem };
    const auto submission = CreateRef<FrameSubmission>();
    FrameSubmissionPublisher publisher;

    ASSERT_TRUE(submission->Submit(instance));
    publisher.Publish(submission);
    publisher.Remove(instance);

    EXPECT_TRUE(publisher.Acquire()->IsEmpty());
}