#include <gtest/gtest.h>

#include <Engine/Aether/FrameSubmission.h>

using namespace Elixir;
using namespace Elixir::Aether;

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
    EXPECT_EQ(submission.GetInstances()[0], &firstInstance);
    EXPECT_EQ(submission.GetInstances()[1], &secondInstance);
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