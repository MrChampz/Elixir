#include <Engine/Animation/BlendNode.h>
#include "TwoBoneIKTestUtils.h"
#include "AnimationCurveTestFixture.h"
#include "AnimationTestNodes.h"
#include "AnimationTestPlayback.h"

#include <limits>

using namespace Elixir;
using AnimationCurveTest = Elixir::Tests::Curves::AnimationCurveFixture;

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
    const auto action = AnimationMontage::Create(
        {
            .Name = "Action",
            .Sequence = Constant(10),
            .BlendIn = 0.4f,
            .BlendOut = 0.2f
        }
    );
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
    const auto action = AnimationMontage::Create(
        {
            .Name = "Action",
            .Sequence = Ramp,
            .BlendIn = 0,
            .BlendOut = 0.1f,
            .Sections = {{"First", 0}, {"Second", 0.5f}}
        }
    );
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
