#include <gtest/gtest.h>

#include <barrier>
#include <thread>

#include <Engine/Aether/Rendering/FrameSubmission.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Rendering;

TEST(FrameSubmissionPublisherTest, PublishesOnlySealedSubmissions)
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

TEST(FrameSubmissionPublisherTest, RemovesDestroyedInstanceFromPublishedFrame)
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

TEST(FrameSubmissionPublisherTest, FiltersInstanceRejectedAtPublication)
{
    const auto system = CreateRef<SCompiledSystem>();
    const SystemInstance instance{ system };
    const auto submission = CreateRef<FrameSubmission>();
    FrameSubmissionPublisher publisher;

    ASSERT_TRUE(submission->Submit(instance));
    publisher.Publish(submission, [](const SSystemInstanceKey&)
    {
        return false;
    });

    EXPECT_TRUE(publisher.Acquire()->IsEmpty());
}

TEST(FrameSubmissionPublisherTest, PublishesAndAcquiresSealedFramesConcurrently)
{
    const auto system = CreateRef<SCompiledSystem>();
    const SystemInstance instance{ system };
    const auto firstSubmission = CreateRef<FrameSubmission>();
    const auto replacementSubmission = CreateRef<FrameSubmission>();
    FrameSubmissionPublisher publisher;
    std::barrier firstFramePublished{ 2 };
    std::barrier beginConcurrentAccess{ 2 };

    ASSERT_TRUE(firstSubmission->Submit(instance));
    ASSERT_TRUE(replacementSubmission->Submit(instance));

    std::thread publicationThread([&]
    {
        publisher.Publish(firstSubmission);
        firstFramePublished.arrive_and_wait();
        beginConcurrentAccess.arrive_and_wait();
        publisher.Publish(replacementSubmission);
    });

    firstFramePublished.arrive_and_wait();
    beginConcurrentAccess.arrive_and_wait();
    const auto concurrentFrame = publisher.Acquire();

    publicationThread.join();

    ASSERT_TRUE(concurrentFrame);
    EXPECT_TRUE(concurrentFrame->IsSealed());
    EXPECT_EQ(concurrentFrame->GetInstanceCount(), 1);

    const auto finalFrame = publisher.Acquire();
    ASSERT_TRUE(finalFrame);
    EXPECT_TRUE(finalFrame->IsSealed());
    EXPECT_EQ(finalFrame->GetInstanceCount(), 1);
}
