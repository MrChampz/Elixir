#include "AnimationTestNodes.h"
#include "../Mesh/SkinningTestData.h"
#include <Engine/Animation/AnimationPlayer.h>
#include <gtest/gtest.h>

using namespace Elixir;

TEST(AnimationPlayerTest, EvaluationPublishesPoseClocksAndNotificationsOnlyWhenApplied)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto sequence = Sequence::Create("Move", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 0,
        .Translations = {{0, {0, 0, 0}}, {1, {2, 0, 0}}}
    }}, {}, {{"Step", 0.1f}});
    ASSERT_TRUE(sequence);
    AnimationPlayer player("Standalone", mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(player.Play(MakeNode(sequence)));

    SAnimationPlayerEvaluation evaluation;
    ASSERT_TRUE(player.Evaluate(0.25f, evaluation));
    EXPECT_FLOAT_EQ(player.GetTime(), 0);
    EXPECT_FLOAT_EQ(player.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    EXPECT_TRUE(player.GetNotifyEvents().empty());
    EXPECT_FLOAT_EQ(evaluation.LocalPose.LocalTransforms[0].Translation.x, 0.5f);
    ASSERT_EQ(evaluation.Notifies.Events.size(), 1);
    EXPECT_EQ(evaluation.Notifies.Events[0].LayerName, "Standalone");

    const SRootMotionIntervalContext interval{
        .Reference = mesh->GetData().ReferencePose.LocalTransforms[0],
        .DeltaSeconds = 0.25f
    };
    SRootMotionDelta movement;
    ASSERT_TRUE(player.ExtractRootMotion(interval, movement, evaluation));
    EXPECT_FLOAT_EQ(movement.Translation.x, 0.5f);
    EXPECT_FLOAT_EQ(player.GetTime(), 0);

    player.Apply(std::move(evaluation));
    EXPECT_FLOAT_EQ(player.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(player.GetLocalPose().LocalTransforms[0].Translation.x, 0.5f);
    ASSERT_EQ(player.GetNotifyEvents().size(), 1);
    EXPECT_EQ(player.GetNotifyEvents()[0].GetName(), "Step");
}

TEST(AnimationPlayerTest, CopiedPlayersKeepIndependentClocksAndMontageState)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto sequence = Sequence::Create("Move", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 0,
        .Translations = {{0, {0, 0, 0}}, {1, {2, 0, 0}}}
    }});
    ASSERT_TRUE(sequence);
    AnimationPlayer original("Standalone", mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(original.Play(MakeNode(sequence)));
    auto copy = original;
    const auto montage = AnimationMontage::Create({
        .Name = "Action",
        .Sequence = sequence,
        .BlendIn = 0,
        .BlendOut = 0
    });
    ASSERT_TRUE(montage);
    ASSERT_TRUE(copy.PlayMontage(montage));
    SAnimationPlayerEvaluation evaluation;
    ASSERT_TRUE(copy.Evaluate(0.25f, evaluation));
    copy.Apply(std::move(evaluation));

    EXPECT_FLOAT_EQ(copy.GetTime(), 0.25f);
    EXPECT_DOUBLE_EQ(copy.GetMontageTime(), 0.25);
    EXPECT_EQ(copy.GetMontage(), montage);
    EXPECT_FLOAT_EQ(original.GetTime(), 0);
    EXPECT_FALSE(original.GetMontage());
    EXPECT_FLOAT_EQ(original.GetLocalPose().LocalTransforms[0].Translation.x, 0);
}
