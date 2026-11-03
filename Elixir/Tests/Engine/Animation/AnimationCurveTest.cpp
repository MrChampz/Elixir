#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>
#include "AnimationCurveTestFixture.h"
#include "AnimationTestNodes.h"
#include "AnimationTestPlayback.h"

#include <limits>

using namespace Elixir;
using AnimationCurveTest = Elixir::Tests::Curves::AnimationCurveFixture;

TEST_F(AnimationCurveTest, SamplesClampedLinearStepAndSingleKeyTracks)
{
    const auto clip = Ramp->WithCurves({
        {"Linear", {{0.2f, -4}, {0.8f, 8}}},
        {"Step", {{0, 2}, {0.5f, 6}, {1, 10}}, EAnimationCurveInterpolation::Step},
        {"Single", {{0.4f, -3}}}
    });
    ASSERT_TRUE(clip);
    AnimationCurveValues values;
    ASSERT_TRUE(clip->SampleCurves(0.5f, values));
    EXPECT_NEAR(values.GetValue("Linear"), 2, 1e-5f);
    EXPECT_FLOAT_EQ(values.GetValue("Step"), 6);
    EXPECT_FLOAT_EQ(values.GetValue("Single"), -3);
    EXPECT_FLOAT_EQ(values.GetValue("Missing"), 0);
    ASSERT_TRUE(clip->SampleCurves(-10, values));
    EXPECT_FLOAT_EQ(values.GetValue("Linear"), -4);
    ASSERT_TRUE(clip->SampleCurves(10, values));
    EXPECT_FLOAT_EQ(values.GetValue("Linear"), 8);
    EXPECT_FLOAT_EQ(values.GetValue("Step"), 10);
    EXPECT_FALSE(clip->SampleCurves(std::numeric_limits<float>::quiet_NaN(), values));
    EXPECT_FLOAT_EQ(values.GetValue("Step"), 10);
}

TEST_F(AnimationCurveTest, RejectsInvalidNamesKeysAndInterpolation)
{
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<std::vector<SAnimationCurve>> invalid = {
        {{"", {{0, 1}}}}, {{"Grip", {}}},
        {{"Grip", {{0, 1}}}, {"Grip", {{1, 2}}}},
        {{"Grip", {{0, nan}}}}, {{"Grip", {{nan, 1}}}},
        {{"Grip", {{-0.1f, 1}}}}, {{"Grip", {{1.1f, 1}}}},
        {{"Grip", {{0.5f, 1}, {0.5f, 2}}}},
        {{"Grip", {{1, 1}, {0, 2}}}},
        {{"Grip", {{0, 1}}, EAnimationCurveInterpolation(255)}}
    };
    for (const auto& curves : invalid) EXPECT_FALSE(Ramp->WithCurves(curves));
    const auto still = Sequence::Create(
        "Still",
        Mesh->GetSkeleton(),
        0,
        {},
        {},
        {},
        {{"Grip", {{0, 7}}}}
    );
    ASSERT_TRUE(still);
    AnimationCurveValues values;
    ASSERT_TRUE(still->SampleCurves(1, values));
    EXPECT_FLOAT_EQ(values.GetValue("Grip"), 7);
}

TEST_F(AnimationCurveTest, MetadataCopiesRetainCurvesAndReplacingCurvesKeepsMetadata)
{
    const auto clip = Ramp->WithSyncMarkers({{"Contact", 0.25f}})
        ->WithNotifies({{"Hit", 0.5f}, {"Window", 0.2f, 0.5f}});
    ASSERT_TRUE(clip);
    ASSERT_EQ(clip->GetCurves().size(), 1);
    const auto copy = clip->WithCurves({{"Other", {{0, 3}}}});
    ASSERT_TRUE(copy);
    EXPECT_EQ(copy->GetSyncMarkers().size(), 1);
    EXPECT_EQ(copy->GetNotifies().size(), 2);
    EXPECT_FLOAT_EQ(copy->GetNotifies()[0].Duration, 0.5f);
    AnimationCurveValues values;
    ASSERT_TRUE(clip->SampleCurves(0.5f, values));
    EXPECT_FLOAT_EQ(values.GetValue("Grip"), 0.5f);
    ASSERT_TRUE(copy->SampleCurves(0.5f, values));
    EXPECT_FLOAT_EQ(values.GetValue("Grip"), 0);
    EXPECT_FLOAT_EQ(values.GetValue("Other"), 3);
}

TEST_F(AnimationCurveTest, LinearInterpolationHandlesOppositeFloatExtremes)
{
    const auto max = std::numeric_limits<float>::max();
    const auto clip = Ramp->WithCurves({{"Grip", {{0, -max}, {1, max}}}});
    ASSERT_TRUE(clip);
    AnimationCurveValues values;
    ASSERT_TRUE(clip->SampleCurves(0.5f, values));
    EXPECT_FLOAT_EQ(values.GetValue("Grip"), 0);
}

TEST_F(AnimationCurveTest, SamplesBlendSpaceWithMissingCurvesAsZeroAndSharedMarkerTimes)
{
    const auto first = Ramp->WithSyncMarkers({{"A", 0}, {"B", 0.5f}});
    const auto second = Sequence::Create(
        "Second",
        Mesh->GetSkeleton(),
        2,
        {},
        {{"A", 0}, {"B", 1.5f}},
        {},
        {{"Grip", {{0, 0}, {2, 2}}}}
    );
    const auto space = BlendSpace1D::Create("Space", {{0, first}, {1, second}});
    ASSERT_TRUE(space);
    auto& instance = *Playback;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), {0.5f}, 0, false));
    ASSERT_TRUE(instance.Seek(space->GetDuration(0.5f) * 0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    const auto empty = Ramp->WithCurves({});
    const auto missing = BlendSpace1D::Create("Missing", {{0, Constant(4)}, {1, empty}});
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(missing), {0.75f}));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
}

TEST_F(AnimationCurveTest, SamplesTwoDimensionalInteriorWeights)
{
    const auto space = BlendSpace2D::Create(
        "Space",
        {
            {{0, 0}, Constant(0)},
            {{1, 0}, Constant(4)},
            {{0, 1}, Constant(8)}
        }
    );
    ASSERT_TRUE(space);
    AnimationCurveValues values;
    const std::array coordinates = {0.25f, 0.25f};
    ASSERT_TRUE(space->SampleCurves(coordinates, 0.5f, values));
    EXPECT_NEAR(values.GetValue("Grip"), 3, 1e-5f);
    EXPECT_FALSE(space->SampleCurves(std::span(coordinates).first(1), 0.5f, values));
    EXPECT_NEAR(values.GetValue("Grip"), 3, 1e-5f);
}

TEST(AnimationCurveValuesTest, InvalidMutationAndOverflowAreTransactionalAndAliasesWork)
{
    AnimationCurveValues values;
    ASSERT_TRUE(values.SetValue("Value", 4));
    EXPECT_FALSE(values.SetValue("", 5));
    EXPECT_FALSE(values.SetValue("Value", std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(values.Blend({}, -1));
    EXPECT_FLOAT_EQ(values.GetValue("Value"), 4);
    ASSERT_TRUE(values.Blend(values, 0.5f));
    EXPECT_FLOAT_EQ(values.GetValue("Value"), 4);
    ASSERT_TRUE(values.ApplyLayer(values, 0.5f, true));
    EXPECT_FLOAT_EQ(values.GetValue("Value"), 6);
    EXPECT_FALSE(values.ApplyLayer({}, std::numeric_limits<float>::quiet_NaN(), false));
    EXPECT_FLOAT_EQ(values.GetValue("Value"), 6);
}

TEST_F(AnimationCurveTest, PlaybackSamplesPoseAndScalarChannelsTogether)
{
    AnimationPlayback playback(MakeNode(Ramp));
    ASSERT_TRUE(playback.IsValid());
    SAnimationEvaluation evaluation;
    ASSERT_TRUE(playback.Sample(0.75, Mesh->GetData().ReferencePose, evaluation));
    EXPECT_DOUBLE_EQ(evaluation.Phase, 0.75);
    EXPECT_FLOAT_EQ(evaluation.Sample.Curves.GetValue("Grip"), 0.75f);
    EXPECT_EQ(
        evaluation.Sample.Pose.LocalTransforms.size(),
        Mesh->GetSkeleton()->GetBones().size()
    );
    SAnimationSample sample;
    ASSERT_TRUE(playback.Sample(0.75, Mesh->GetData().ReferencePose, sample));
    EXPECT_FLOAT_EQ(sample.Curves.GetValue("Grip"), 0.75f);

    SLocalPose pose;
    ASSERT_TRUE(playback.Sample(0.75, Mesh->GetData().ReferencePose, pose));
    for (size_t bone = 0; bone < pose.LocalTransforms.size(); ++bone)
        EXPECT_TRUE(pose.LocalTransforms[bone].IsNearlyEqual(
            evaluation.Sample.Pose.LocalTransforms[bone]
        ));
}

TEST_F(AnimationCurveTest, BlendSpacePoseCurvesAndPlaybackShareTheSamplingTimeline)
{
    for (const bool synchronized : {false, true})
    {
        SCOPED_TRACE(synchronized);
        auto first = Sequence::Create(
            "First",
            Mesh->GetSkeleton(),
            1,
            {{.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {1, 0, 0}}}}},
            {},
            {},
            {{"Grip", {{0, 0}, {1, 1}}}}
        );
        auto second = Sequence::Create(
            "Second",
            Mesh->GetSkeleton(),
            2,
            {{.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {2, {2, 0, 0}}}}},
            {},
            {},
            {{"Grip", {{0, 0}, {2, 2}}}}
        );
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        if (synchronized)
        {
            first = first->WithSyncMarkers({{"A", 0}, {"B", 0.5f}});
            second = second->WithSyncMarkers({{"A", 0}, {"B", 1.5f}});
            ASSERT_TRUE(first);
            ASSERT_TRUE(second);
        }
        const auto space = BlendSpace1D::Create("Timeline", {{0, first}, {1, second}});
        ASSERT_TRUE(space);
        ASSERT_TRUE(PlayWithInputs(*Playback, MakeNode(space), {0.5f}, 0, false));
        const std::array coordinates{0.5f};
        for (const float phase : {-1.0f, 0.0f, 0.25f, 0.5f, 0.9f, 1.0f, 2.0f})
        {
            SCOPED_TRACE(phase);
            SLocalPose pose;
            AnimationCurveValues curves;
            ASSERT_TRUE(space->Sample(coordinates, phase, pose));
            ASSERT_TRUE(space->SampleCurves(coordinates, phase, curves));
            const auto expected = curves.GetValue("Grip");
            EXPECT_NEAR(pose.LocalTransforms[0].Translation.x, expected, 1e-5f);
            const auto time = space->GetDuration(coordinates) * std::clamp(phase, 0.0f, 1.0f);
            ASSERT_TRUE(Playback->Seek(time));
            EXPECT_NEAR(Playback->GetCurveValues().GetValue("Grip"), expected, 1e-5f);
            EXPECT_NEAR(
                Playback->GetLocalPose().LocalTransforms[0].Translation.x,
                expected,
                1e-5f
            );
            if (phase == 0.5f)
                EXPECT_NEAR(expected, synchronized ? 1.0f : 0.75f, 1e-5f);
        }
    }
}
