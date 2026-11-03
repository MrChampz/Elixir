#include "AnimationInertializationTestFixture.h"
#include <Engine/Animation/AnimationInertialization.h>

using namespace Elixir;
using namespace Elixir::Tests::Inertia;
using AnimationInertializationCaptureTest = AnimationInertializationFixture;

TEST_F(AnimationInertializationCaptureTest, SamplesPreserveInitialMotionWithoutPlayback)
{
    SAnimationSample source, previous, target, next;
    const auto& reference = Mesh->GetData().ReferencePose;
    ASSERT_TRUE(Moving->Sample(0.2f, source.Pose, reference));
    ASSERT_TRUE(Moving->SampleCurves(0.2f, source.Curves));
    ASSERT_TRUE(Moving->Sample(0.1f, previous.Pose, reference));
    ASSERT_TRUE(Moving->SampleCurves(0.1f, previous.Curves));
    ASSERT_TRUE(Target->Sample(0, target.Pose, reference));
    ASSERT_TRUE(Target->SampleCurves(0, target.Curves));
    ASSERT_TRUE(Target->Sample(0.01f, next.Pose, reference));
    ASSERT_TRUE(Target->SampleCurves(0.01f, next.Curves));

    AnimationInertialization inertia;
    EXPECT_FALSE(inertia.IsCaptured());
    ASSERT_TRUE(inertia.Capture(source, &previous, 0.1f, target, next, 0.01f));
    EXPECT_TRUE(inertia.IsCaptured());
    auto sample = target;
    ASSERT_TRUE(inertia.Apply(0, 0.5f, sample));
    EXPECT_EQ(
        sample.Pose.LocalTransforms[0].Translation,
        source.Pose.LocalTransforms[0].Translation
    );
    EXPECT_EQ(sample.Curves.GetValues(), source.Curves.GetValues());

    ASSERT_TRUE(Target->Sample(0.0001f, sample.Pose, reference));
    ASSERT_TRUE(Target->SampleCurves(0.0001f, sample.Curves));
    ASSERT_TRUE(inertia.Apply(0.0001, 0.5f, sample));
    EXPECT_NEAR(sample.Pose.LocalTransforms[0].Translation.x, 0.4002f, 1e-5f);
    EXPECT_NEAR(sample.Curves.GetValue("Value"), 0.4002f, 1e-5f);

    sample = next;
    ASSERT_TRUE(inertia.Apply(0.5, 0.5f, sample));
    EXPECT_EQ(
        sample.Pose.LocalTransforms[0].Translation,
        next.Pose.LocalTransforms[0].Translation
    );
    EXPECT_EQ(sample.Curves.GetValues(), next.Curves.GetValues());
}

TEST_F(AnimationInertializationCaptureTest, RecaptureDiscardsOffsetsForRemovedCurves)
{
    SAnimationSample source{Mesh->GetData().ReferencePose};
    const auto target = source;
    ASSERT_TRUE(source.Curves.SetValue("Removed", 2));
    AnimationInertialization inertia;
    ASSERT_TRUE(inertia.Capture(source, nullptr, 0, target, target, 0.01f));
    auto sample = target;
    ASSERT_TRUE(inertia.Apply(0.1, 0.5f, sample));
    EXPECT_GT(sample.Curves.GetValue("Removed"), 0);

    ASSERT_TRUE(inertia.Capture(target, nullptr, 0, target, target, 0.01f));
    sample = target;
    ASSERT_TRUE(inertia.Apply(0.1, 0.5f, sample));
    EXPECT_TRUE(sample.Curves.GetValues().empty());
}

TEST_F(AnimationInertializationCaptureTest, InvalidSamplesPreservePreviousCapture)
{
    SAnimationSample source{Mesh->GetData().ReferencePose};
    auto target = source;
    source.Pose.LocalTransforms[0].Translation.x = 2;
    AnimationInertialization inertia;
    ASSERT_TRUE(inertia.Capture(source, nullptr, 0, target, target, 0.01f));
    auto invalid = target;
    invalid.Pose.LocalTransforms[0].Scale.x = 0;
    EXPECT_FALSE(inertia.Capture(source, nullptr, 0, target, invalid, 0.01f));
    EXPECT_FALSE(inertia.Capture(source, nullptr, 0, target, target, 0));
    ASSERT_TRUE(inertia.Apply(0, 0.5f, target));
    EXPECT_FLOAT_EQ(target.Pose.LocalTransforms[0].Translation.x, 2);
}
