#include "AnimationTestNodes.h"
#include "../Mesh/SkinningTestData.h"
#include <Engine/Animation/AnimationPlayer.h>
#include <Engine/Animation/BlendNode.h>
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

TEST(AnimationPlayerTest, DiscardedSuppressedEvaluationDoesNotChangePublishedNotifyWindows)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto sequence = Sequence::Create("Window", mesh->GetSkeleton(), 1, {}, {}, {
        {"Active", 0, 1}
    });
    const auto composition = BlendNode::Create("Composition", {
        {.Name = "Child", .Node = MakeNode(sequence)}
    });
    ASSERT_TRUE(composition);
    AnimationPlayer player("Standalone", mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(player.Play(MakeNode(composition)));

    SAnimationPlayerEvaluation first;
    ASSERT_TRUE(player.Evaluate(0.125f, first));
    player.Apply(std::move(first));
    ASSERT_EQ(player.GetNotifyEvents().size(), 2);
    const auto activation = player.GetNotifyEvents()[0].ActivationId;
    ASSERT_NE(activation, 0);

    SAnimationPlayerEvaluation discarded;
    ASSERT_TRUE(player.Evaluate(0.125f, discarded, false, false));
    EXPECT_FLOAT_EQ(player.GetTime(), 0.125f);
    ASSERT_EQ(player.GetNotifyEvents().size(), 2);
    EXPECT_EQ(player.GetNotifyEvents()[0].ActivationId, activation);

    SAnimationPlayerEvaluation accepted;
    ASSERT_TRUE(player.Evaluate(0.125f, accepted));
    ASSERT_EQ(accepted.Notifies.Events.size(), 1);
    EXPECT_EQ(accepted.Notifies.Events[0].Type, EAnimationNotifyEventType::Update);
    EXPECT_EQ(accepted.Notifies.Events[0].ActivationId, activation);
    player.Apply(std::move(accepted));
    EXPECT_FLOAT_EQ(player.GetTime(), 0.25f);
    ASSERT_EQ(player.GetNotifyEvents().size(), 1);
    EXPECT_EQ(player.GetNotifyEvents()[0].ActivationId, activation);
}

TEST(AnimationPlayerTest, FullySuppressedNestedSourcesResumeWithFreshNotifyWindows)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto sequence = Sequence::Create("Window", mesh->GetSkeleton(), 1, {}, {}, {
        {"Active", 0, 1}
    });
    const auto inner = BlendNode::Create("Inner", {
        {.Name = "Child", .Node = MakeNode(sequence)}
    });
    const auto outer = BlendNode::Create("Outer", {
        {.Name = "Nested", .Node = MakeNode(inner)}
    });
    ASSERT_TRUE(inner);
    ASSERT_TRUE(outer);
    AnimationPlayer player("Standalone", mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(player.Play(MakeNode(outer)));
    SAnimationPlayerEvaluation first;
    ASSERT_TRUE(player.Evaluate(0.125f, first));
    player.Apply(std::move(first));
    ASSERT_EQ(player.GetNotifyEvents().size(), 2);
    const auto activation = player.GetNotifyEvents()[0].ActivationId;

    const auto action = Sequence::Create("Silent", mesh->GetSkeleton(), 0.625f, {});
    const auto montage = AnimationMontage::Create({
        .Name = "Suppress",
        .Sequence = action,
        .BlendIn = 0,
        .BlendOut = 0
    });
    ASSERT_TRUE(montage);
    ASSERT_TRUE(player.PlayMontage(montage));
    for (int frame = 0; frame < 2; ++frame)
    {
        SAnimationPlayerEvaluation evaluation;
        ASSERT_TRUE(player.Evaluate(0.25f, evaluation));
        player.Apply(std::move(evaluation));
        if (frame == 0)
        {
            ASSERT_EQ(player.GetNotifyEvents().size(), 1);
            EXPECT_EQ(player.GetNotifyEvents()[0].Type, EAnimationNotifyEventType::End);
            EXPECT_TRUE(player.GetNotifyEvents()[0].Cancelled);
            EXPECT_EQ(player.GetNotifyEvents()[0].ActivationId, activation);
        }
        else
        {
            EXPECT_TRUE(player.GetNotifyEvents().empty());
        }
    }

    EXPECT_TRUE(player.GetMontage());
    SAnimationPlayerEvaluation resumed;
    ASSERT_TRUE(player.Evaluate(0.25f, resumed));
    player.Apply(std::move(resumed));
    EXPECT_FALSE(player.GetMontage());
    ASSERT_EQ(player.GetNotifyEvents().size(), 2);
    EXPECT_EQ(player.GetNotifyEvents()[0].Type, EAnimationNotifyEventType::Begin);
    EXPECT_DOUBLE_EQ(player.GetNotifyEvents()[0].TimeOffset, 0.125);
    EXPECT_NE(player.GetNotifyEvents()[0].ActivationId, activation);
    EXPECT_EQ(player.GetNotifyEvents()[1].Type, EAnimationNotifyEventType::Update);
    EXPECT_DOUBLE_EQ(player.GetNotifyEvents()[1].TimeOffset, 0.25);
    EXPECT_DOUBLE_EQ(player.GetNotifyEvents()[1].DeltaSeconds, 0.125);
    EXPECT_EQ(
        player.GetNotifyEvents()[1].ActivationId,
        player.GetNotifyEvents()[0].ActivationId
    );

    SAnimationPlayerEvaluation next;
    ASSERT_TRUE(player.Evaluate(0.0625f, next));
    ASSERT_EQ(next.Notifies.Events.size(), 1);
    EXPECT_EQ(next.Notifies.Events[0].Type, EAnimationNotifyEventType::Update);
    EXPECT_EQ(next.Notifies.Events[0].ActivationId, player.GetNotifyEvents()[0].ActivationId);
}

TEST(AnimationPlayerTest, PlaybackRetainsItsNotifyPolicyAcrossAdvancesAndIndependentCopies)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto sequence = Sequence::Create("Window", mesh->GetSkeleton(), 1, {}, {}, {
        {"Active", 0, 1}
    });
    const auto composition = BlendNode::Create("Composition", {
        {.Name = "Child", .Node = MakeNode(sequence)}
    });
    ASSERT_TRUE(composition);
    AnimationPlayback playback(MakeNode(composition));
    ASSERT_TRUE(playback.BindParameters(CreateRef<AnimationParameters>()));
    playback.SetNotifiesEnabled(false);

    for (int frame = 0; frame < 2; ++frame)
    {
        const auto phase = playback.Advance(0.125f, mesh->GetData().ReferencePose);
        ASSERT_TRUE(std::isfinite(phase));
        playback.Apply(phase);
        const auto* child = playback.GetLayer("Child");
        ASSERT_TRUE(child);
        EXPECT_TRUE(child->GetPlayer().GetNotifyEvents().empty());
    }

    auto copy = playback;
    playback.SetNotifiesEnabled(true);
    const auto phase = playback.Advance(0.125f, mesh->GetData().ReferencePose);
    ASSERT_TRUE(std::isfinite(phase));
    playback.Apply(phase);
    const auto* child = playback.GetLayer("Child");
    ASSERT_TRUE(child);
    ASSERT_EQ(child->GetPlayer().GetNotifyEvents().size(), 2);
    EXPECT_EQ(child->GetPlayer().GetNotifyEvents()[0].Type, EAnimationNotifyEventType::Begin);
    EXPECT_EQ(child->GetPlayer().GetNotifyEvents()[1].Type, EAnimationNotifyEventType::Update);

    const auto copiedPhase = copy.Advance(0.125f, mesh->GetData().ReferencePose);
    ASSERT_TRUE(std::isfinite(copiedPhase));
    copy.Apply(copiedPhase);
    const auto* copiedChild = copy.GetLayer("Child");
    ASSERT_TRUE(copiedChild);
    EXPECT_TRUE(copiedChild->GetPlayer().GetNotifyEvents().empty());
    EXPECT_EQ(child->GetPlayer().GetNotifyEvents().size(), 2);
}
