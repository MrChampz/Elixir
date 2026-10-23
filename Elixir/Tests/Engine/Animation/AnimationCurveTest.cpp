#include <gtest/gtest.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>
#include <Engine/Animation/AnimationControllerInstance.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    class AnimationCurveTest : public testing::Test
    {
    protected:
        /** @brief Create a mesh and a curve-only sequence with a one-second cycle. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Ramp = AnimationSequence::Create("Ramp", Mesh->GetSkeleton(), 1, {}, {}, {}, {},
                {{"Grip", {{0, 0}, {1, 1}}}});
            ASSERT_TRUE(Ramp);
        }

        /**
         * @brief Make a constant curve-only clip on the fixture skeleton.
         * @param value Scalar value held over the complete cycle.
         * @param name Curve name.
         */
        Ref<AnimationSequence> Constant(float value, std::string name = "Grip") const
        {
            return Ramp->WithCurves({{std::move(name), {{0, value}}}});
        }

        Ref<SkeletalMesh> Mesh;
        Ref<AnimationSequence> Ramp;
    };
}

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
    const auto still = AnimationSequence::Create("Still", Mesh->GetSkeleton(), 0, {}, {}, {}, {},
        {{"Grip", {{0, 7}}}});
    ASSERT_TRUE(still);
    AnimationCurveValues values;
    ASSERT_TRUE(still->SampleCurves(1, values));
    EXPECT_FLOAT_EQ(values.GetValue("Grip"), 7);
}

TEST_F(AnimationCurveTest, MetadataCopiesRetainCurvesAndReplacingCurvesKeepsMetadata)
{
    const auto clip = Ramp->WithSyncMarkers({{"Contact", 0.25f}})
        ->WithNotifies({{"Hit", 0.5f}})->WithNotifyStates({{"Window", 0.2f, 0.7f}});
    ASSERT_TRUE(clip);
    ASSERT_EQ(clip->GetCurves().size(), 1);
    const auto copy = clip->WithCurves({{"Other", {{0, 3}}}});
    ASSERT_TRUE(copy);
    EXPECT_EQ(copy->GetSyncMarkers().size(), 1);
    EXPECT_EQ(copy->GetNotifies().size(), 1);
    EXPECT_EQ(copy->GetNotifyStates().size(), 1);
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
    const auto second = AnimationSequence::Create("Second", Mesh->GetSkeleton(), 2, {},
        {{"A", 0}, {"B", 1.5f}}, {}, {}, {{"Grip", {{0, 0}, {2, 2}}}});
    const auto space = BlendSpace1D::Create("Space", {{0, first}, {1, second}});
    ASSERT_TRUE(space);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(space, {0.5f}, 0, false));
    ASSERT_TRUE(instance.Seek(space->GetDuration(0.5f) * 0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    const auto empty = Ramp->WithCurves({});
    const auto missing = BlendSpace1D::Create("Missing", {{0, Constant(4)}, {1, empty}});
    ASSERT_TRUE(instance.Play(missing, {0.75f}));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
}

TEST_F(AnimationCurveTest, SamplesTwoDimensionalInteriorWeights)
{
    const auto space = BlendSpace2D::Create("Space", {
        {{0, 0}, Constant(0)}, {{1, 0}, Constant(4)}, {{0, 1}, Constant(8)}});
    ASSERT_TRUE(space);
    AnimationCurveValues values;
    const std::array coordinates = {0.25f, 0.25f};
    ASSERT_TRUE(space->SampleCurves(coordinates, 0.5f, values));
    EXPECT_NEAR(values.GetValue("Grip"), 3, 1e-5f);
    EXPECT_FALSE(space->SampleCurves(std::span(coordinates).first(1), 0.5f, values));
    EXPECT_NEAR(values.GetValue("Grip"), 3, 1e-5f);
}

TEST_F(AnimationCurveTest, PlaybackLoopsSeeksPausesAndResetsWithPose)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Ramp));
    ASSERT_TRUE(instance.Update(1.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.25f);
    ASSERT_TRUE(instance.Seek(0.75f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.75f);
    instance.Pause();
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.75f);
    instance.Resume();
    ASSERT_TRUE(instance.Play(Ramp, false));
    ASSERT_TRUE(instance.Update(2));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    ASSERT_TRUE(instance.SetLocalPose(Mesh->GetData().ReferencePose));
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
    ASSERT_TRUE(instance.Play(Ramp));
    instance.Stop();
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
}

TEST_F(AnimationCurveTest, InterruptedFadesCaptureVisibleCurveValues)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(0)));
    ASSERT_TRUE(instance.Play(Constant(8), 1.0f));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    ASSERT_TRUE(instance.Play(Constant(10), 1.0f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(instance.Play(Ramp->WithCurves({}), 1.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 3);
}

TEST_F(AnimationCurveTest, LayersOverrideAuthoredNamesAndAddFromZeroRegardlessOfBoneWeight)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(4)));
    const auto mask = BoneMask::Create("Partial", Mesh->GetSkeleton(), {0, 0.1f});
    ASSERT_TRUE(instance.AddLayer("Overlay", mask, 0.5f));
    auto* layer = instance.GetLayer("Overlay");
    ASSERT_TRUE(layer->Play(Constant(8)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    EXPECT_FLOAT_EQ(layer->GetCurveValues().GetValue("Grip"), 8);
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 8);
    ASSERT_TRUE(layer->Play(Constant(3, "Recoil")));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 4);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Recoil"), 1.5f);
    ASSERT_TRUE(layer->SetWeight(0));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Recoil"), 0);
    ASSERT_TRUE(instance.RemoveLayer("Overlay"));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 4);
}

TEST_F(AnimationCurveTest, OverflowRejectsFrameWithoutAdvancingPoseCurvesClocksOrEvents)
{
    const auto max = std::numeric_limits<float>::max();
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(max)));
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Add", mask));
    auto* layer = instance.GetLayer("Add");
    ASSERT_TRUE(layer->Play(Constant(max)->WithNotifies({{"Hit", 0.1f}})));
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(layer->GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), max);
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(layer->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.25f);
}

TEST_F(AnimationCurveTest, MontageCurvesUseEntranceReplacementAndExplicitExitEnvelopes)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(2)));
    const auto action = AnimationMontage::Create({.Name = "Action", .Sequence = Constant(10),
        .BlendIn = 0.4f, .BlendOut = 0.2f});
    ASSERT_TRUE(action);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(layer.PlayMontage(action));
    EXPECT_FLOAT_EQ(layer.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(instance.Update(0.2f));
    // Source 2 blended toward captured 6 by 0.5, then action 10 by 0.5.
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 7);
    ASSERT_TRUE(layer.StopMontage(0.2f));
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    EXPECT_FALSE(layer.GetMontage());
}

TEST_F(AnimationCurveTest, MontageSectionJumpAndNaturalCompletionSampleCurrentTimeline)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(2)));
    const auto action = AnimationMontage::Create({.Name = "Action", .Sequence = Ramp,
        .BlendIn = 0, .BlendOut = 0.1f,
        .Sections = {{"First", 0}, {"Second", 0.5f}}});
    ASSERT_TRUE(action);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(action));
    ASSERT_TRUE(layer.JumpToMontageSection("Second"));
    EXPECT_FLOAT_EQ(layer.GetCurveValues().GetValue("Grip"), 0.5f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.75f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    EXPECT_FALSE(layer.GetMontage());
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

TEST_F(AnimationCurveTest, CurveBindingControlsIKInTheSameFrameAndClampsOnlyItsWeight)
{
    auto data = Mesh->GetData();
    data.Skeleton = Skeleton::Create("Limb", {
        {.Name = "Root"}, {.Name = "Middle", .ParentIndex = 0,
            .ReferenceTransform = {.Translation = {1, 0, 0}}},
        {.Name = "End", .ParentIndex = 1,
            .ReferenceTransform = {.Translation = {1, 0, 0}}}
    });
    data.ReferencePose.clear();
    const auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    SkeletalMeshInstance instance(mesh);
    const auto clip = AnimationSequence::Create("IK", mesh->GetSkeleton(), 1, {}, {}, {}, {},
        {{"Grip", {{0, -1}, {1, 3}}}});
    ASSERT_TRUE(instance.Play(clip, false));
    STwoBoneIKConstraint constraint{
        .EndBoneIndex = 2, .TargetPosition = {1, 1, 0}, .PolePosition = {0, 2, 0},
        .WeightCurve = "Grip"
    };
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0);
    EXPECT_FLOAT_EQ(instance.GetModelPose()[2][3].x, 2);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    EXPECT_NEAR(instance.GetModelPose()[2][3].x, 1, 1e-4f);
    EXPECT_NEAR(instance.GetModelPose()[2][3].y, 1, 1e-4f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 3);
    EXPECT_NEAR(instance.GetModelPose()[2][3].y, 1, 1e-4f);
    constraint.WeightCurve = "Missing";
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetModelPose()[2][3].x, 2);
    constraint.WeightCurve = "";
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
}

TEST_F(AnimationCurveTest, ControllerStateTransitionsBlendCurvesWithoutExtraParameters)
{
    const auto controller = AnimationController::Create({
        .Name = "Controller", .Parameters = {{"Attack", false}},
        .Layers = {{.StateMachine = {
            .InitialState = "Idle",
            .States = {{.Name = "Idle", .Sequence = Constant(2)},
                {.Name = "Attack", .Sequence = Constant(10)}},
            .Transitions = {{.From = "Idle", .To = "Attack",
                .Conditions = {{"Attack", EAnimationComparison::Equal, true}},
                .FadeDuration = 1}}
        }}}
    });
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(mesh.GetCurveValues().GetValue("Grip"), 2);
    ASSERT_TRUE(instance.SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(mesh.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(mesh.GetCurveValues().GetValue("Grip"), 10);
}

TEST_F(AnimationCurveTest, EmptyBoneMaskSuppressesScalarLayerContribution)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(4)));
    const auto mask = BoneMask::Create("None", Mesh->GetSkeleton(), {0, 0});
    ASSERT_TRUE(instance.AddLayer("Muted", mask));
    ASSERT_TRUE(instance.GetLayer("Muted")->Play(Constant(10)));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 4);
    EXPECT_FLOAT_EQ(instance.GetLayer("Muted")->GetTime(), 0.5f);
}

TEST_F(AnimationCurveTest, FailedPoseCompositionRetainsPreviouslyPublishedCurves)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Constant(2)));
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Overlay", mask, 0.5f));
    const auto clip = AnimationSequence::Create("Reflection", Mesh->GetSkeleton(), 1,
        {{.BoneIndex = 0, .Scales = {{0, {-1, -1, -1}}}}}, {}, {}, {},
        {{"Grip", {{0, 10}}}});
    ASSERT_TRUE(clip);
    ASSERT_TRUE(instance.GetLayer("Overlay")->Play(clip));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetLayer("Overlay")->GetTime(), 0);
}
