#include "AnimationInertializationTestFixture.h"
#include "AnimationTestNodes.h"
#include <Engine/Animation/BlendSpace1D.h>
#include <limits>

using namespace Elixir;
using namespace Elixir::Tests::Inertia;
using AnimationInertializationTest = AnimationInertializationFixture;

TEST_F(AnimationInertializationTest, ReleasesOutgoingAssetAndDoesNotEvaluateItDuringTransition)
{
    auto source = Clip(2);
    std::weak_ptr<const Sequence> weak = source;
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(source)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(Target), 0.5f, false, Inertialization));
    source.reset();
    EXPECT_TRUE(weak.expired());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(instance.GetBaseLayer().GetPlayer().IsTransitioning());
}

TEST_F(
    AnimationInertializationTest,
    PauseAndZeroDeltaUpdatesDoNotConsumeHistoryOrTransitionTime
)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(Target), 0.5f, false, Inertialization));
    instance.Pause();
    const auto pose = instance.GetModelPose().Matrices;
    ASSERT_TRUE(instance.Update(10));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetBaseLayer().GetPlayer().IsTransitioning());
    instance.Resume();
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0.2002f, 1e-5f);
}

TEST_F(AnimationInertializationTest, SeekManualPoseAndStopClearOffsetsAndVelocityHistory)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(Target), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Seek(1));
    EXPECT_FALSE(instance.GetBaseLayer().GetPlayer().IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 7);
    ASSERT_TRUE(instance.Play(MakeNode(Clip(4)), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 7, 1e-5f);
    ASSERT_TRUE(instance.SetLocalPose(Mesh->GetData().ReferencePose));
    ASSERT_TRUE(instance.Play(MakeNode(Clip(4)), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0, 1e-5f);
    instance.Stop();
    EXPECT_FALSE(instance.GetBaseLayer().GetPlayer().IsTransitioning());
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
}

TEST_F(AnimationInertializationTest, BlendSpaceDestinationUsesCurrentCoordinatesAndPhase)
{
    const auto space = BlendSpace1D::Create("Space", {{0, Clip(10)}, {1, Clip(20)}});
    ASSERT_TRUE(space);
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().Play(
        MakeNode(space), {0.5f}, 0.5f, false, Inertialization
    ));
    ASSERT_TRUE(instance.RefreshPose());
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 15);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"), 15);
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().SetSourceInputs({1}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 20);
}

TEST_F(
    AnimationInertializationTest,
    RootMotionUsesDestinationImmediatelyAndRootPoseStaysLocked
)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(Clip(10, 6)), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 0.6f, 1e-5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0);
}

TEST_F(AnimationInertializationTest, OnlyDestinationEventsAreEmittedDuringInertialization)
{
    auto& instance = *Playback;
    const auto source = Moving->WithNotifies({{"Source", 0.2f}, {"Window", 0, 1}});
    const auto target = Target->WithNotifies({{"Target", 0.1f}});
    ASSERT_TRUE(instance.Play(MakeNode(source)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(target), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto& events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].GetName(), "Window");
    EXPECT_EQ(events[0].Type, EAnimationNotifyEventType::End);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[1].GetName(), "Target");
    EXPECT_EQ(events[1].Type, EAnimationNotifyEventType::Instant);
}

TEST_F(AnimationInertializationTest, InvalidModesAndDurationsPreserveAcceptedPlayback)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Play(MakeNode(Target), 1.0f, false, EAnimationTransitionMode(255)));
    EXPECT_FALSE(instance.Play(MakeNode(Target), -1.0f, false, Inertialization));
    EXPECT_FALSE(instance.Play(
        MakeNode(Target), std::numeric_limits<float>::infinity(), false, Inertialization
    ));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.1f);
    EXPECT_FALSE(instance.GetBaseLayer().GetPlayer().IsTransitioning());
    ASSERT_TRUE(instance.Play(MakeNode(Target), 0.0f, false, Inertialization));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 10);
}

TEST_F(AnimationInertializationTest, StateMachineSelectsInertializationAndRejectsUnknownModes)
{
    SStateMachineCreateInfo info{
        .InitialState = "Source",
        .States =
            {{.Name = "Source", .Node = MakeNode(Moving)},
             {.Name = "Target", .Node = MakeNode(Target)}},
        .Transitions = {
            {.From = "Source",
             .To = "Target",
             .Conditions = {{"Change", EAnimationComparison::Equal, true}},
             .FadeDuration = 0.5f,
             .Mode = Inertialization}
        }
    };
    const auto machine = StateMachine::Create(info);
    ASSERT_TRUE(machine);
    auto& instance = *Playback;
    ASSERT_TRUE(instance.SetParameters({{"Change", false}}));
    ASSERT_TRUE(instance.Play(MakeNode(machine)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.GetParameters().SetBool("Change", true));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0.2002f, 1e-5f);
    EXPECT_EQ(instance.GetCurrentState(), "Target");
    info.Transitions[0].Mode = EAnimationTransitionMode(255);
    EXPECT_FALSE(StateMachine::Create(std::move(info)));
}

TEST_F(
    AnimationInertializationTest,
    FailedCompositionDoesNotConsumeTransitionClocksOrVelocityHistory
)
{
    auto& instance = *Playback;
    const auto controlHandle = CreatePlayback();
    ASSERT_TRUE(controlHandle);
    auto& control = *controlHandle;
    for (auto* mesh : {&instance, &control})
    {
        ASSERT_TRUE(mesh->Play(MakeNode(Moving)));
        ASSERT_TRUE(mesh->Update(0.1f));
        ASSERT_TRUE(mesh->Play(MakeNode(Target), 0.5f, false, Inertialization));
    }
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("InvalidComposition", mask, 0.5f));
    const auto reflection = Sequence::Create(
        "Reflection", Mesh->GetSkeleton(), 5, {{.BoneIndex = 0, .Scales = {{0, {-1, -1, -1}}}}}
    );
    ASSERT_TRUE(
        instance.GetLayer("InvalidComposition")->GetPlayer().Play(MakeNode(reflection))
    );
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Update(0.1f));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"), 0.2f);
    ASSERT_TRUE(instance.RemoveLayer("InvalidComposition"));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(control.Update(0.1f));
    EXPECT_EQ(
        instance.GetLocalPose().LocalTransforms[0].Translation,
        control.GetLocalPose().LocalTransforms[0].Translation
    );
    EXPECT_FLOAT_EQ(
        instance.GetCurveValues().GetValue("Value"), control.GetCurveValues().GetValue("Value")
    );
}

TEST_F(AnimationInertializationTest, CompletedStationarySourceDoesNotReuseStaleVelocity)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving), false));
    ASSERT_TRUE(instance.Update(5));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(instance.IsPlaying());
    ASSERT_TRUE(instance.Play(MakeNode(Clip(0)), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 10, 1e-5f);
}

TEST_F(AnimationInertializationTest, CrossFadeCanEnterAndLeaveAnInertialTransition)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(0))));
    ASSERT_TRUE(instance.Play(MakeNode(Clip(8)), 1.0f));
    ASSERT_TRUE(instance.Update(0.2f));
    const auto previous = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    ASSERT_TRUE(instance.Update(0.001f));
    const auto current = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    const auto velocity = (current - previous) / 0.001f;
    ASSERT_TRUE(instance.Play(MakeNode(Target), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(
        (instance.GetLocalPose().LocalTransforms[0].Translation.x - current) / 0.0001f,
        velocity, 0.02f
    );
    const auto smoothed = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(0)), 0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, smoothed);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_NEAR(
        instance.GetLocalPose().LocalTransforms[0].Translation.x, smoothed * 0.5f, 1e-5f
    );
}

TEST_F(
    AnimationInertializationTest,
    StateMachineInterruptionsPreserveAcceptedVelocityAndCurves
)
{
    const auto machine = StateMachine::Create(
        {.InitialState = "Source",
         .States =
             {{.Name = "Source", .Node = MakeNode(Moving)},
              {.Name = "Target", .Node = MakeNode(Target)},
              {.Name = "Third", .Node = MakeNode(Clip(-5, 1))}},
         .Transitions = {
             {.From = "Source",
              .To = "Target",
              .Conditions = {{"Change", EAnimationComparison::Equal, true}},
              .FadeDuration = 0.5f,
              .Mode = Inertialization},
             {.From = "Target",
              .To = "Third",
              .Conditions = {{"Again", EAnimationComparison::Equal, true}},
              .FadeDuration = 0.5f,
              .Mode = Inertialization}
         }}
    );
    ASSERT_TRUE(machine);
    ASSERT_TRUE(Playback->SetParameters({{"Change", false}, {"Again", false}}));
    ASSERT_TRUE(Playback->Play(MakeNode(machine)));
    ASSERT_TRUE(Playback->Update(0.1f));
    ASSERT_TRUE(Playback->GetParameters().SetBool("Change", true));
    ASSERT_TRUE(Playback->Update(0.2f));
    const auto previous = Playback->GetLocalPose().LocalTransforms[0].Translation.x;
    ASSERT_TRUE(Playback->Update(0.001f));
    const auto current = Playback->GetLocalPose().LocalTransforms[0].Translation.x;
    const auto velocity = (current - previous) / 0.001f;
    ASSERT_TRUE(Playback->GetParameters().SetBool("Again", true));
    ASSERT_TRUE(Playback->Update(0.0001f));
    const auto next = Playback->GetLocalPose().LocalTransforms[0].Translation.x;
    EXPECT_NEAR((next - current) / 0.0001f, velocity, 0.02f);
    EXPECT_NEAR(Playback->GetCurveValues().GetValue("Value"), next, 1e-5f);
    EXPECT_EQ(Playback->GetCurrentState(), "Third");
}

TEST_F(AnimationInertializationTest, StateMachineSeekDiscardsPreviousVelocityHistory)
{
    const auto machine = StateMachine::Create(
        {.InitialState = "Source",
         .States =
             {{.Name = "Source", .Node = MakeNode(Moving)},
              {.Name = "Target", .Node = MakeNode(Clip(4))}},
         .Transitions = {
             {.From = "Source",
              .To = "Target",
              .Conditions = {{"Change", EAnimationComparison::Equal, true}},
              .FadeDuration = 0.5f,
              .Mode = Inertialization}
         }}
    );
    ASSERT_TRUE(machine);
    ASSERT_TRUE(Playback->SetParameters({{"Change", false}}));
    ASSERT_TRUE(Playback->Play(MakeNode(machine)));
    ASSERT_TRUE(Playback->Update(0.1f));
    ASSERT_TRUE(Playback->Seek(1));
    EXPECT_FLOAT_EQ(Playback->GetLocalPose().LocalTransforms[0].Translation.x, 2);
    ASSERT_TRUE(Playback->GetParameters().SetBool("Change", true));
    ASSERT_TRUE(Playback->Update(0.0001f));
    EXPECT_NEAR(Playback->GetLocalPose().LocalTransforms[0].Translation.x, 2, 1e-5f);
    EXPECT_NEAR(Playback->GetCurveValues().GetValue("Value"), 2, 1e-5f);
}

TEST_F(AnimationInertializationTest, StateMachineUsesDestinationRootMotionDuringSmoothing)
{
    const auto machine = StateMachine::Create(
        {.InitialState = "Source",
         .States =
             {{.Name = "Source", .Node = MakeNode(Moving)},
              {.Name = "Target", .Node = MakeNode(Clip(10, 6))}},
         .Transitions = {
             {.From = "Source",
              .To = "Target",
              .Conditions = {{"Change", EAnimationComparison::Equal, true}},
              .FadeDuration = 0.5f,
              .Mode = Inertialization}
         }}
    );
    ASSERT_TRUE(machine);
    ASSERT_TRUE(Playback->SetParameters({{"Change", false}}));
    ASSERT_TRUE(Playback->SetRootMotionEnabled(true));
    ASSERT_TRUE(Playback->Play(MakeNode(machine)));
    ASSERT_TRUE(Playback->Update(0.1f));
    ASSERT_TRUE(Playback->GetParameters().SetBool("Change", true));
    ASSERT_TRUE(Playback->Update(0.1f));
    EXPECT_NEAR(Playback->GetRootMotionDelta().Translation.x, 0.6f, 1e-5f);
    EXPECT_FLOAT_EQ(Playback->GetLocalPose().LocalTransforms[0].Translation.x, 0);
}
