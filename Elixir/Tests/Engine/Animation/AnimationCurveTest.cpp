#include <gtest/gtest.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>
#include "TwoBoneIKTestUtils.h"
#include "AnimationTestNodes.h"
#include <Engine/Animation/BlendNode.h>
#include "AnimationTestPlayback.h"
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;
using namespace Elixir::Tests::IK;

namespace
{
    /** Uses candidate frame curves to correct a pose and reject excessive values. */
    class CurveDrivenTranslation final : public PoseModifier
    {
    public:
        /**
         * @brief Move the first bone using Grip values no greater than one half.
         * @param context Candidate frame curves borrowed from the animation.
         * @param pose Candidate transforms modified only for accepted values.
         */
        bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const override
        {
            if (!context.Curves) return false;
            const auto value = context.Curves->GetValue("Grip");
            if (value > 0.5f) return false;
            pose.LocalTransforms[0].Translation.x = value;
            return true;
        }
    };

    class AnimationCurveTest : public testing::Test
    {
    protected:
        /** @brief Create a mesh and a curve-only sequence with a one-second cycle. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Ramp = Sequence::Create(
                "Ramp",
                Mesh->GetSkeleton(),
                1,
                {},
                {},
                {},
                {{"Grip", {{0, 0}, {1, 1}}}}
            );
            ASSERT_TRUE(Ramp);
            Playback = MakeIKAnimation(Mesh);
            ASSERT_TRUE(Playback);
        }

        /**
         * @brief Make a constant curve-only clip on the fixture skeleton.
         * @param value Scalar value held over the complete cycle.
         * @param name Curve name.
         */
        Ref<Sequence> Constant(float value, std::string name = "Grip") const
        {
            return Ramp->WithCurves({{std::move(name), {{0, value}}}});
        }

        Ref<SkeletalMesh> Mesh;
        Ref<Sequence> Ramp;
        Ref<Animation> Playback;
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
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Ramp)));
    ASSERT_TRUE(instance.Update(1.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.25f);
    ASSERT_TRUE(instance.Seek(0.75f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.75f);
    instance.Pause();
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.75f);
    instance.Resume();
    ASSERT_TRUE(instance.Play(MakeNode(Ramp), false));
    ASSERT_TRUE(instance.Update(2));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    ASSERT_TRUE(instance.SetLocalPose(Mesh->GetData().ReferencePose));
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
    ASSERT_TRUE(instance.Play(MakeNode(Ramp)));
    instance.Stop();
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
}

TEST_F(AnimationCurveTest, InterruptedFadesCaptureVisibleCurveValues)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(0))));
    ASSERT_TRUE(instance.Play(MakeNode(Constant(8)), 1.0f));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    ASSERT_TRUE(instance.Play(MakeNode(Constant(10)), 1.0f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(instance.Play(MakeNode(Ramp->WithCurves({})), 1.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 3);
}

TEST_F(AnimationCurveTest, LayersOverrideAuthoredNamesAndAddFromZeroRegardlessOfBoneWeight)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(4))));
    const auto mask = BoneMask::Create("Partial", Mesh->GetSkeleton(), {0, 0.1f});
    ASSERT_TRUE(instance.AddLayer("Overlay", mask, 0.5f));
    auto* layer = instance.GetLayer("Overlay");
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(Constant(8))));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetCurveValues().GetValue("Grip"), 8);
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 8);
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(Constant(3, "Recoil"))));
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
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(max))));
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Add", mask));
    auto* layer = instance.GetLayer("Add");
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(Constant(max)->WithNotifies({{"Hit", 0.1f}}))));
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), max);
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(layer->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.25f);
}

TEST_F(AnimationCurveTest, MontageCurvesUseEntranceReplacementAndExplicitExitEnvelopes)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(2))));
    const auto action = AnimationMontage::Create({.Name = "Action", .Sequence = Constant(10),
        .BlendIn = 0.4f, .BlendOut = 0.2f});
    ASSERT_TRUE(action);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(action));
    EXPECT_FLOAT_EQ(layer.GetPlayer().GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(instance.Update(0.2f));
    // Source 2 blended toward captured 6 by 0.5, then action 10 by 0.5.
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 7);
    ASSERT_TRUE(layer.GetPlayer().StopMontage(0.2f));
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    EXPECT_FALSE(layer.GetPlayer().GetMontage());
}

TEST_F(AnimationCurveTest, MontageSectionJumpAndNaturalCompletionSampleCurrentTimeline)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(2))));
    const auto action = AnimationMontage::Create({.Name = "Action", .Sequence = Ramp,
        .BlendIn = 0, .BlendOut = 0.1f,
        .Sections = {{"First", 0}, {"Second", 0.5f}}});
    ASSERT_TRUE(action);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(action));
    ASSERT_TRUE(layer.GetPlayer().JumpToMontageSection("Second"));
    EXPECT_FLOAT_EQ(layer.GetPlayer().GetCurveValues().GetValue("Grip"), 0.5f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0.75f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    EXPECT_FALSE(layer.GetPlayer().GetMontage());
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
    data.Skeleton = Skeleton::Create(
        "Limb",
        {
            {.Name = "Root"}, {.Name = "Middle", .ParentIndex = 0,
                .ReferenceTransform = {.Translation = {1, 0, 0}}},
            {.Name = "End", .ParentIndex = 1,
                .ReferenceTransform = {.Translation = {1, 0, 0}}}
        }
    );
    data.ReferencePose.LocalTransforms.clear();
    const auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    const auto handle = MakeIKAnimation(mesh);
    ASSERT_TRUE(handle);
    auto& instance = *handle;
    const auto solver = AttachIK(instance);
    ASSERT_TRUE(solver);
    const auto clip = Sequence::Create(
        "IK",
        mesh->GetSkeleton(),
        1,
        {},
        {},
        {},
        {{"Grip", {{0, -1}, {1, 3}}}}
    );
    ASSERT_TRUE(instance.Play(MakeNode(clip), false));
    STwoBoneIKConstraint constraint{
        .EndBoneIndex = 2, .TargetPosition = {1, 1, 0}, .PolePosition = {0, 2, 0},
        .WeightCurve = "Grip"
    };
    ASSERT_TRUE(solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 0);
    EXPECT_FLOAT_EQ(instance.GetModelPose().Matrices[2][3].x, 2);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 1);
    EXPECT_NEAR(instance.GetModelPose().Matrices[2][3].x, 1, 1e-4f);
    EXPECT_NEAR(instance.GetModelPose().Matrices[2][3].y, 1, 1e-4f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 3);
    EXPECT_NEAR(instance.GetModelPose().Matrices[2][3].y, 1, 1e-4f);
    constraint.WeightCurve = "Missing";
    ASSERT_TRUE(solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetModelPose().Matrices[2][3].x, 2);
    constraint.WeightCurve = "";
    EXPECT_FALSE(solver->SetConstraints(std::span(&constraint, 1)));
}

TEST_F(AnimationCurveTest, StateMachineTransitionsBlendCurvesWithoutExtraParameters)
{
    const auto machine = StateMachine::Create({
        .InitialState = "Idle",
        .States = {
            {.Name = "Idle", .Node = MakeNode(Constant(2))},
            {.Name = "Attack", .Node = MakeNode(Constant(10))}
        },
        .Transitions = {{
            .From = "Idle", .To = "Attack",
            .Conditions = {{"Attack", EAnimationComparison::Equal, true}},
            .FadeDuration = 1
        }}
    });
    ASSERT_TRUE(machine);
    auto& instance = *Playback;
    ASSERT_TRUE(instance.SetParameters({{"Attack", false}}));
    ASSERT_TRUE(instance.Play(MakeNode(machine)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    ASSERT_TRUE(instance.GetParameters().SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 6);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 10);
}

TEST_F(AnimationCurveTest, EmptyBoneMaskSuppressesScalarLayerContribution)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(4))));
    const auto mask = BoneMask::Create("None", Mesh->GetSkeleton(), {0, 0});
    ASSERT_TRUE(instance.AddLayer("Muted", mask));
    ASSERT_TRUE(instance.GetLayer("Muted")->GetPlayer().Play(MakeNode(Constant(10))));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 4);
    EXPECT_FLOAT_EQ(instance.GetLayer("Muted")->GetPlayer().GetTime(), 0.5f);
}

TEST_F(AnimationCurveTest, FailedPoseCompositionRetainsPreviouslyPublishedCurves)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Constant(2))));
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Overlay", mask, 0.5f));
    const auto clip = Sequence::Create(
        "Reflection",
        Mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Scales = {{0, {-1, -1, -1}}}}},
        {},
        {},
        {{"Grip", {{0, 10}}}}
    );
    ASSERT_TRUE(clip);
    ASSERT_TRUE(instance.GetLayer("Overlay")->GetPlayer().Play(MakeNode(clip)));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Grip"), 2);
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetLayer("Overlay")->GetPlayer().GetTime(), 0);
}

TEST_F(AnimationCurveTest, NestedBlendNodesComposeCurvesWithTheirChildPoses)
{
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(mask);
    const auto inner = BlendNode::Create(
        "Inner",
        {
            {.Name = "Base", .Node = MakeNode(Constant(2))},
            {.Name = "Overlay", .Mask = mask, .Weight = 0.5f, .Node = MakeNode(Constant(10))}
        }
    );
    ASSERT_TRUE(inner);
    const auto outer = BlendNode::Create(
        "Outer",
        {
            {.Name = "Base", .Node = inner},
            {
                .Name = "Add", .Mask = mask, .Weight = 0.5f,
                .BlendMode = EAnimationLayerBlendMode::Additive,
                .Node = MakeNode(Constant(4))
            }
        }
    );
    ASSERT_TRUE(outer);
    ASSERT_TRUE(Playback->Play(outer));
    EXPECT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Grip"), 8);
    ASSERT_TRUE(Playback->Update(0.25f));
    EXPECT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Grip"), 8);
    ASSERT_TRUE(Playback->Seek(0.5f));
    EXPECT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Grip"), 8);
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

TEST_F(AnimationCurveTest, ModifiersReadCandidateCurvesBeforeAtomicFramePublication)
{
    const std::array<Ref<const PoseModifier>, 1> modifiers{
        CreateRef<CurveDrivenTranslation>()
    };
    ASSERT_TRUE(Playback->SetPoseModifiers(modifiers));
    ASSERT_TRUE(Playback->Play(MakeNode(Ramp->WithNotifies({{"Hit", 0.5f}}))));
    ASSERT_TRUE(Playback->Update(0.25f));
    EXPECT_FLOAT_EQ(Playback->GetFrame().Sample.Curves.GetValue("Grip"), 0.25f);
    EXPECT_FLOAT_EQ(Playback->GetLocalPose().LocalTransforms[0].Translation.x, 0.25f);

    EXPECT_FALSE(Playback->Update(0.5f));
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Grip"), 0.25f);
    const auto& playerCurves = Playback->GetBaseLayer().GetPlayer().GetCurveValues();
    EXPECT_FLOAT_EQ(playerCurves.GetValue("Grip"), 0.25f);
    EXPECT_FLOAT_EQ(Playback->GetLocalPose().LocalTransforms[0].Translation.x, 0.25f);
    EXPECT_TRUE(Playback->GetNotifyEvents().empty());

    ASSERT_TRUE(Playback->SetLocalPose(Mesh->GetData().ReferencePose));
    EXPECT_TRUE(Playback->GetFrame().Sample.Curves.GetValues().empty());
    Playback->Stop();
    EXPECT_TRUE(Playback->GetCurveValues().GetValues().empty());
}
