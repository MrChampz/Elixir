#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include <gtest/gtest.h>
#include "AnimationGraphTestDriver.h"
#include <Engine/Animation/BlendSpace1D.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    class AnimationMontageTest : public testing::Test
    {
    protected:
        /** @brief Create a shared two-bone mesh. */
        void SetUp() override { Mesh = SkinningTests::MakeMesh(); ASSERT_TRUE(Mesh); }

        /**
         * @brief Create a one-second root translation with optional events.
         * @param distance Translation along X at the end.
         * @param offset Translation along X at the start.
         * @param notifies Events on the clip timeline.
         */
        Ref<Sequence> Clip(
            float distance, float offset = 0, std::vector<SAnimationNotify> notifies = {}
        )
        {
            return Sequence::Create("Clip", Mesh->GetSkeleton(), 1, {{
                .BoneIndex = 0,
                .Translations = {{0, {offset, 0, 0}}, {1, {offset + distance, 0, 0}}}
            }}, {}, std::move(notifies));
        }

        /**
         * @brief Create an action sharing the fixture skeleton.
         * @param in Entrance seconds.
         * @param out Exit seconds.
         * @param interruptible Allow replacement and explicit stop.
         */
        Ref<AnimationMontage> Action(float in = 0.2f, float out = 0.2f, bool interruptible = true)
        {
            return AnimationMontage::Create({
                .Name = "Action", .Sequence = Clip(2, 10),
                .BlendIn = in, .BlendOut = out, .Interruptible = interruptible
            });
        }

        Ref<SkeletalMesh> Mesh;
    };
}

TEST_F(AnimationMontageTest, AssetRejectsInvalidSettings)
{
    EXPECT_FALSE(AnimationMontage::Create({}));
    EXPECT_FALSE(AnimationMontage::Create({.Name = "Empty", .Sequence =
        Sequence::Create("Empty", Mesh->GetSkeleton(), 0, {})}));
    EXPECT_FALSE(AnimationMontage::Create({.Name = "Invalid", .Sequence = Clip(1),
        .BlendIn = -1}));
    EXPECT_FALSE(AnimationMontage::Create({.Name = "Invalid", .Sequence = Clip(1),
        .BlendOut = std::numeric_limits<float>::infinity()}));
    EXPECT_TRUE(Action(2, 2));
}

TEST_F(AnimationMontageTest, SourceKeepsAdvancingAndReturnsWithoutRestarting)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(1))));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Action()));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-6f);
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 5.15f, 1e-5f);
    EXPECT_NEAR(player.GetTime(), 0.1f, 1e-6f);
    ASSERT_TRUE(instance.Update(0.8f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 6.35f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.2f));
    EXPECT_FALSE(player.GetMontage());
    EXPECT_NEAR(player.GetTime(), 0.1f, 1e-5f);
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0.1f, 1e-5f);
}

TEST_F(AnimationMontageTest, UnassignedAndCompletedLayersCanPlayActions)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Action(0, 0)));
    EXPECT_TRUE(instance.IsPlaying());
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 11);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(player.GetMontage());
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(instance.Play(MakeNode(Clip(1)), false));
    ASSERT_TRUE(instance.Update(1));
    ASSERT_TRUE(player.PlayMontage(Action()));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 11);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 1);
}

TEST_F(AnimationMontageTest, InstanceAndLayerPauseFreezeActionAndEvents)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Action()));
    instance.Pause();
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0);
    instance.Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    player.Pause();
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0.25);
    player.Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0.5);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationMontageTest, ReplacementStartsAtVisiblePoseAndObeysInterruptionPolicy)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Action(0, 0)));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto previous = player.GetLocalPose().LocalTransforms[0].Translation.x;
    const auto replacement = Action(0.2f, 0.2f, false);
    ASSERT_TRUE(player.PlayMontage(replacement));
    EXPECT_FLOAT_EQ(player.GetLocalPose().LocalTransforms[0].Translation.x, previous);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, previous);
    EXPECT_FALSE(player.PlayMontage(Action()));
    EXPECT_FALSE(player.StopMontage());
    EXPECT_EQ(player.GetMontage(), replacement);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(player.GetMontage());
}

TEST_F(AnimationMontageTest, ExplicitExitStartsAtCurrentWeightAndDoesNotRestart)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Action()));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(player.StopMontage(0.2f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-6f);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.25f, 1e-6f);
    ASSERT_TRUE(player.StopMontage(1));
    ASSERT_TRUE(instance.Update(0.11f));
    EXPECT_FALSE(player.GetMontage());
    ASSERT_TRUE(player.PlayMontage(Action()));
    EXPECT_FALSE(player.StopMontage(-1));
    EXPECT_TRUE(player.GetMontage());
    ASSERT_TRUE(player.StopMontage(0));
    EXPECT_FALSE(player.GetMontage());
}

TEST_F(AnimationMontageTest, EventsUseActionTimelineThenResumeSourceWithinTheSameFrame)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(1, 0, {{"Source", 0.2f}}))));
    auto action = AnimationMontage::Create({.Name = "Events", .Sequence =
        Clip(2, 0, {{"Start", 0}, {"Hit", 0.5f}, {"End", 1}})});
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(action));
    ASSERT_TRUE(instance.Update(1.3f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 4);
    EXPECT_EQ(events[0].GetName(), "Start");
    EXPECT_EQ(events[1].GetName(), "Hit");
    EXPECT_EQ(events[2].GetName(), "End");
    EXPECT_EQ(events[3].GetName(), "Source");
    EXPECT_NEAR(events[3].TimeOffset, 1.2, 1e-5);
    EXPECT_EQ(events[1].LayerName, "Base");
    EXPECT_EQ(events[1].Sequence, action->GetSequence());
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationMontageTest, MutedMaskedLayerStillCompletesWithoutEvents)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Action", mask, 0));
    auto* layer = instance.GetLayer("Action");
    const auto action = AnimationMontage::Create({.Name = "Muted", .Sequence =
        Clip(2, 0, {{"Hit", 0.5f}})});
    ASSERT_TRUE(layer->GetPlayer().PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_DOUBLE_EQ(layer->GetPlayer().GetMontageTime(), 0.5);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(layer->GetPlayer().GetMontage());
}

TEST_F(AnimationMontageTest, FailedCompositionPreservesActionClockPoseAndPreviousEvents)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(Action()));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto before = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Overflow", mask));
    auto* layer = instance.GetLayer("Overflow");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    auto pose = Mesh->GetData().ReferencePose;
    pose.LocalTransforms[0].Translation.x = std::numeric_limits<float>::max();
    ASSERT_TRUE(layer->GetPlayer().SetLocalPose(pose));
    ASSERT_TRUE(instance.AddLayer("OverflowAgain", mask));
    layer = instance.GetLayer("OverflowAgain");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(layer->GetPlayer().SetLocalPose(pose));
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_DOUBLE_EQ(instance.GetBaseLayer().GetPlayer().GetMontageTime(), 0.25);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, before);
}

TEST_F(AnimationMontageTest, RootMotionBlendsVelocitiesAndHandlesExitWithinLargeFrame)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(1))));
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(Action()));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FALSE(instance.GetBaseLayer().GetPlayer().GetMontage());
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 2.3f, 1e-5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(Action(0, 0)));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 1, 1e-5f);
}

TEST_F(AnimationMontageTest, ControllerChangesUnderlyingSourceWithoutOverwritingAction)
{
    const auto a = Clip(1), b = Clip(3);
    auto controller = CreateTestGraph({.Name = "Machine",
        .Parameters = {{"Switch", false}}, .Layers = {{.Node = MakeNode(StateMachine::Create({
            .InitialState = "A", .States = {{.Name = "A", .Node = MakeNode(a)},
                {.Name = "B", .Node = MakeNode(b)}},
            .Transitions = {{.From = "A", .To = "B",
                .Conditions = {{"Switch", EAnimationComparison::Equal, true}}}}
        }))}}});
    ASSERT_TRUE(controller);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    AnimationGraphTestDriver animation(controller, instance);
    ASSERT_TRUE(animation.Update(0));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Action(0, 0)));
    ASSERT_TRUE(animation.SetBool("Switch", true));
    ASSERT_TRUE(animation.Update(0.5f));
    EXPECT_EQ(animation.GetCurrentState(), "B");
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 11);
    ASSERT_TRUE(animation.Update(0.6f));
    EXPECT_FALSE(player.GetMontage());
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0.3f, 1e-5f);
}

TEST_F(AnimationMontageTest, SharedAssetHasIndependentClocksAndExplicitResetCancelsAction)
{
    const auto firstHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(firstHandle);
    auto& first = *firstHandle;
    const auto secondHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    const auto action = Action();
    ASSERT_TRUE(first.GetBaseLayer().GetPlayer().PlayMontage(action));
    ASSERT_TRUE(second.GetBaseLayer().GetPlayer().PlayMontage(action));
    ASSERT_TRUE(first.Update(0.5f));
    ASSERT_TRUE(second.Update(0.25f));
    EXPECT_DOUBLE_EQ(first.GetBaseLayer().GetPlayer().GetMontageTime(), 0.5);
    EXPECT_DOUBLE_EQ(second.GetBaseLayer().GetPlayer().GetMontageTime(), 0.25);
    first.Stop();
    EXPECT_FALSE(first.GetBaseLayer().GetPlayer().GetMontage());
    ASSERT_TRUE(second.SetLocalPose(Mesh->GetData().ReferencePose));
    EXPECT_FALSE(second.GetBaseLayer().GetPlayer().GetMontage());
    EXPECT_FALSE(second.GetBaseLayer().GetPlayer().PlayMontage(nullptr));
}

TEST_F(AnimationMontageTest, UnassignedLayerEmitsTypedEventsOnceAndPauseDoesNotConsumeStart)
{
    struct SHit { int Damage = 7; };
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    const auto action = AnimationMontage::Create({.Name = "Hit", .Sequence =
        Clip(1, 0, {{"Hit", 0, 0, SHit{}}})});
    ASSERT_TRUE(player.PlayMontage(action));
    player.Pause();
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    player.Resume();
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    const auto event = instance.GetNotifyEvents()[0];
    ASSERT_TRUE(event.GetPayload<SHit>());
    EXPECT_EQ(event.GetPayload<SHit>()->Damage, 7);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(player.StopMontage(0));
    EXPECT_EQ(event.GetPayload<SHit>()->Damage, 7);
}

TEST_F(AnimationMontageTest, BlendSpaceCoordinatesContinueUpdatingUnderAction)
{
    const auto space = BlendSpace1D::Create("Locomotion", {{0, Clip(1)}, {1, Clip(3)}});
    ASSERT_TRUE(space);
    const auto controller = CreateTestGraph({.Name = "Direct",
        .Parameters = {{"Speed", 0.0f}}, .Layers = {{
            .Node = MakeNode(space), .InputParameters = {"Speed"}
        }}});
    ASSERT_TRUE(controller);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    AnimationGraphTestDriver animation(controller, instance);
    ASSERT_TRUE(animation.Update(0));
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(Action(0, 0)));
    ASSERT_TRUE(animation.SetFloat("Speed", 1));
    ASSERT_TRUE(animation.Update(1.25f));
    EXPECT_FALSE(instance.GetBaseLayer().GetPlayer().GetMontage());
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0.75f, 1e-5f);
}

TEST_F(AnimationMontageTest, MaskAndAdditiveModeApplyAfterMontagePoseComposition)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto mask = BoneMask::Create("RootHalf", Mesh->GetSkeleton(), {0.5f, 0});
    ASSERT_TRUE(instance.AddLayer("Additive", mask, 0.5f));
    auto* layer = instance.GetLayer("Additive");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(layer->GetPlayer().PlayMontage(Action(0, 0)));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 2.75f);
    EXPECT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation, Mesh->GetData().ReferencePose.LocalTransforms[1].Translation);
}

TEST_F(AnimationMontageTest, CombinedActionAndResumedEventsRespectCapacityTransactionally)
{
    std::vector<SAnimationNotify> notifies(
        Sequence::MaxNotifyEventsPerUpdate, {"Action", 0.5f}
    );
    const auto action = AnimationMontage::Create({.Name = "Many", .Sequence =
        Clip(1, 0, std::move(notifies))});
    ASSERT_TRUE(action);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(1, 0, {{"Source", 0.2f}}))));
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(action));
    EXPECT_FALSE(instance.Update(1.5f));
    EXPECT_DOUBLE_EQ(instance.GetBaseLayer().GetPlayer().GetMontageTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.6f));
    EXPECT_EQ(instance.GetNotifyEvents().size(), Sequence::MaxNotifyEventsPerUpdate);
}
