#include <gtest/gtest.h>
#include <Engine/Animation/AnimationControllerInstance.h>
#include <Engine/Animation/BlendSpace1D.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    constexpr auto Inertialization = EAnimationTransitionMode::Inertialization;

    /**
     * @brief Read a shortest angular displacement in radians.
     * @param rotation Relative unit quaternion.
     */
    glm::vec3 AngularDisplacement(glm::quat rotation)
    {
        if (rotation.w < 0) rotation = -rotation;
        const glm::vec3 axis(rotation.x, rotation.y, rotation.z);
        const auto length = glm::length(axis);
        return length < 1e-7f ? axis * 2.0f
            : axis * (2.0f * std::atan2(length, rotation.w) / length);
    }

    class AnimationInertializationTest : public testing::Test
    {
    protected:
        /** @brief Create a compatible mesh and moving and stationary source assets. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Moving = Clip(0, 2);
            Target = Clip(10, -3);
            ASSERT_TRUE(Moving);
            ASSERT_TRUE(Target);
        }

        /**
         * @brief Create a linear translation and scalar curve over five seconds.
         * @param start Initial root translation and curve value.
         * @param speed Units per second on the root X axis and curve.
         */
        Ref<AnimationSequence> Clip(float start, float speed = 0) const
        {
            return AnimationSequence::Create("Clip", Mesh->GetSkeleton(), 5,
                {{.BoneIndex = 0, .Translations = {{0, {start, 0, 0}},
                    {5, {start + 5 * speed, 0, 0}}}}}, {}, {}, {},
                {{"Value", {{0, start}, {5, start + 5 * speed}}}});
        }

        Ref<SkeletalMesh> Mesh;
        Ref<AnimationSequence> Moving;
        Ref<AnimationSequence> Target;
    };
}

TEST_F(AnimationInertializationTest, PreservesPoseAndOutgoingVelocityAtDestinationChange)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving, false));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto position = instance.GetLocalPose()[0].Translation.x;
    ASSERT_TRUE(instance.Play(Target, 1.0f, false, Inertialization));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, position);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, position);
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR((instance.GetLocalPose()[0].Translation.x - position) / 0.0001f, 2, 0.01f);
    EXPECT_NEAR(instance.GetCurveValues().GetValue("Value"), position + 0.0002f, 1e-5f);
}

TEST_F(AnimationInertializationTest, InterruptedTransitionKeepsRecentVelocityInsteadOfFreezingSource)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(Target, 1.0f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.2f));
    const auto previous = instance.GetLocalPose()[0].Translation.x;
    ASSERT_TRUE(instance.Update(0.001f));
    const auto current = instance.GetLocalPose()[0].Translation.x;
    const auto speed = (current - previous) / 0.001f;
    ASSERT_TRUE(instance.Play(Clip(-5, 1), 0.5f, false, Inertialization));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, current);
    ASSERT_TRUE(instance.Update(0.0001f));
    const auto next = instance.GetLocalPose()[0].Translation.x;
    EXPECT_NEAR((next - current) / 0.0001f, speed, 0.02f);
}

TEST_F(AnimationInertializationTest, ConvergesToDestinationPoseCurvesAndVelocityAtDuration)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(Target, 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.4999f));
    const auto previous = instance.GetLocalPose()[0].Translation.x;
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 8.5f, 1e-5f);
    EXPECT_NEAR(instance.GetCurveValues().GetValue("Value"), 8.5f, 1e-5f);
    EXPECT_NEAR((instance.GetLocalPose()[0].Translation.x - previous) / 0.0001f, -3, 0.02f);
    EXPECT_FALSE(instance.GetBaseLayer().IsTransitioning());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 7.75f, 1e-5f);
}

TEST_F(AnimationInertializationTest, PreservesNoncommutingAngularVelocityAndUnitRotations)
{
    const auto source = AnimationSequence::Create("Source", Mesh->GetSkeleton(), 5,
        {{.BoneIndex = 0, .Rotations = {{0, glm::quat(1, 0, 0, 0)},
            {5, glm::angleAxis(2.5f, glm::vec3(0, 0, 1))}}}});
    const auto targetStart = glm::angleAxis(1.4f, glm::normalize(glm::vec3(1, 2, 0)));
    const auto target = AnimationSequence::Create("Target", Mesh->GetSkeleton(), 5,
        {{.BoneIndex = 0, .Rotations = {{0, targetStart},
            {5, glm::angleAxis(1.0f, glm::vec3(0, 1, 0)) * targetStart}}}});
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(source));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto rotation = instance.GetLocalPose()[0].Rotation;
    ASSERT_TRUE(instance.Play(target, 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    const auto velocity = AngularDisplacement(instance.GetLocalPose()[0].Rotation *
        glm::conjugate(rotation)) / 0.0001f;
    EXPECT_NEAR(velocity.x, 0, 0.005f);
    EXPECT_NEAR(velocity.y, 0, 0.005f);
    EXPECT_NEAR(velocity.z, 0.5f, 0.005f);
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(instance.Update(0.05f));
        EXPECT_TRUE(instance.GetLocalPose()[0].IsValid());
    }
}

TEST_F(AnimationInertializationTest, QuaternionSignsAndHalfTurnsKeepShortestFinitePaths)
{
    const auto halfTurn = glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0));
    const auto first = AnimationSequence::Create("First", Mesh->GetSkeleton(), 1,
        {{.BoneIndex = 0, .Rotations = {{0, halfTurn}}}});
    const auto second = AnimationSequence::Create("Second", Mesh->GetSkeleton(), 1,
        {{.BoneIndex = 0, .Rotations = {{0, -halfTurn}}}});
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(first));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(second, 0.2f, true, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_NEAR(std::abs(glm::dot(instance.GetLocalPose()[0].Rotation, halfTurn)), 1, 1e-5f);
    ASSERT_TRUE(instance.Play(Moving, 0.2f, true, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(instance.GetLocalPose()[0].IsValid());
}

TEST_F(AnimationInertializationTest, SignedLogarithmicScalePreservesReflectionsAndAvoidsZero)
{
    const auto source = AnimationSequence::Create("Source", Mesh->GetSkeleton(), 1,
        {{.BoneIndex = 0, .Scales = {{0, {-2, 2, 2}}}}});
    const auto target = AnimationSequence::Create("Target", Mesh->GetSkeleton(), 1,
        {{.BoneIndex = 0, .Scales = {{0, {-8, 8, 8}}}}});
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(source));
    ASSERT_TRUE(instance.Play(target, 1.0f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Scale.x, -4, 1e-5f);
    EXPECT_NEAR(instance.GetLocalPose()[0].Scale.y, 4, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Scale.x, -8);
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Play(Moving, 0.5f, false, Inertialization));
    EXPECT_EQ(instance.GetModelPose(), pose);
    // An immediate switch intentionally accepts reflection changes.
    ASSERT_TRUE(instance.Play(Moving, 0.0f, false, Inertialization));
}

TEST_F(AnimationInertializationTest, MissingHistoryUsesZeroOutgoingVelocityAndMissingCurvesUseZero)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Clip(2)));
    ASSERT_TRUE(instance.Play(Target->WithCurves({}), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 2, 1e-5f);
    EXPECT_NEAR(instance.GetCurveValues().GetValue("Value"), 2, 1e-5f);
    ASSERT_TRUE(instance.Update(0.4999f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"), 0);
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
}

TEST_F(AnimationInertializationTest, ReleasesOutgoingAssetAndDoesNotEvaluateItDuringTransition)
{
    auto source = Clip(2);
    std::weak_ptr<const AnimationSequence> weak = source;
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(source));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(Target, 0.5f, false, Inertialization));
    source.reset();
    EXPECT_TRUE(weak.expired());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(instance.GetBaseLayer().IsTransitioning());
}

TEST_F(AnimationInertializationTest, PauseAndZeroDeltaUpdatesDoNotConsumeHistoryOrTransitionTime)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(Target, 0.5f, false, Inertialization));
    instance.Pause();
    const auto pose = instance.GetModelPose();
    ASSERT_TRUE(instance.Update(10));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetBaseLayer().IsTransitioning());
    instance.Resume();
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 0.2002f, 1e-5f);
}

TEST_F(AnimationInertializationTest, SeekManualPoseAndStopClearOffsetsAndVelocityHistory)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(Target, 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Seek(1));
    EXPECT_FALSE(instance.GetBaseLayer().IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 7);
    ASSERT_TRUE(instance.Play(Clip(4), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 7, 1e-5f);
    ASSERT_TRUE(instance.SetLocalPose(Mesh->GetData().ReferencePose));
    ASSERT_TRUE(instance.Play(Clip(4), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 0, 1e-5f);
    instance.Stop();
    EXPECT_FALSE(instance.GetBaseLayer().IsTransitioning());
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
}

TEST_F(AnimationInertializationTest, BlendSpaceDestinationUsesCurrentCoordinatesAndPhase)
{
    const auto space = BlendSpace1D::Create("Space", {{0, Clip(10)}, {1, Clip(20)}});
    ASSERT_TRUE(space);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(space, {0.5f}, 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 15);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"), 15);
    ASSERT_TRUE(instance.SetBlendSpaceValue({1}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 20);
}

TEST_F(AnimationInertializationTest, RootMotionUsesDestinationImmediatelyAndRootPoseStaysLocked)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(Clip(10, 6), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 0.6f, 1e-5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 0);
}

TEST_F(AnimationInertializationTest, OnlyDestinationEventsAreEmittedDuringInertialization)
{
    SkeletalMeshInstance instance(Mesh);
    const auto source = Moving->WithNotifies({{"Source", 0.2f}})
        ->WithNotifyStates({{"Window", 0, 1}});
    const auto target = Target->WithNotifies({{"Target", 0.1f}});
    ASSERT_TRUE(instance.Play(source));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(target, 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].GetNotify().Name, "Target");
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
}

TEST_F(AnimationInertializationTest, InvalidModesAndDurationsPreserveAcceptedPlayback)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Play(Target, 1.0f, false, EAnimationTransitionMode(255)));
    EXPECT_FALSE(instance.Play(Target, -1.0f, false, Inertialization));
    EXPECT_FALSE(instance.Play(Target, std::numeric_limits<float>::infinity(), false, Inertialization));
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.1f);
    EXPECT_FALSE(instance.GetBaseLayer().IsTransitioning());
    ASSERT_TRUE(instance.Play(Target, 0.0f, false, Inertialization));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 10);
}

TEST_F(AnimationInertializationTest, ControllerSelectsInertializationAndRejectsUnknownModes)
{
    SAnimationControllerCreateInfo info{
        .Name = "Controller", .Parameters = {{"Change", false}},
        .Layers = {{.StateMachine = {
            .InitialState = "Source",
            .States = {{.Name = "Source", .Sequence = Moving},
                {.Name = "Target", .Sequence = Target}},
            .Transitions = {{.From = "Source", .To = "Target",
                .Conditions = {{"Change", EAnimationComparison::Equal, true}},
                .FadeDuration = 0.5f, .Mode = Inertialization}}
        }}}
    };
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.SetBool("Change", true));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(mesh.GetLocalPose()[0].Translation.x, 0.2002f, 1e-5f);
    EXPECT_EQ(instance.GetCurrentState(), "Target");
    info.Layers[0].StateMachine.Transitions[0].Mode = EAnimationTransitionMode(255);
    EXPECT_FALSE(AnimationController::Create(std::move(info)));
}

TEST_F(AnimationInertializationTest, FailedCompositionDoesNotConsumeTransitionClocksOrVelocityHistory)
{
    SkeletalMeshInstance instance(Mesh), control(Mesh);
    for (auto* mesh : {&instance, &control})
    {
        ASSERT_TRUE(mesh->Play(Moving));
        ASSERT_TRUE(mesh->Update(0.1f));
        ASSERT_TRUE(mesh->Play(Target, 0.5f, false, Inertialization));
    }
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("InvalidComposition", mask, 0.5f));
    const auto reflection = AnimationSequence::Create("Reflection", Mesh->GetSkeleton(), 5,
        {{.BoneIndex = 0, .Scales = {{0, {-1, -1, -1}}}}});
    ASSERT_TRUE(instance.GetLayer("InvalidComposition")->Play(reflection));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Update(0.1f));
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"), 0.2f);
    ASSERT_TRUE(instance.RemoveLayer("InvalidComposition"));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(control.Update(0.1f));
    EXPECT_EQ(instance.GetLocalPose()[0].Translation, control.GetLocalPose()[0].Translation);
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"),
        control.GetCurveValues().GetValue("Value"));
}

TEST_F(AnimationInertializationTest, FrameSubdivisionDoesNotChangeCapturedOffsetDecay)
{
    SkeletalMeshInstance single(Mesh), subdivided(Mesh);
    for (auto* instance : {&single, &subdivided})
    {
        ASSERT_TRUE(instance->Play(Moving));
        ASSERT_TRUE(instance->Update(0.1f));
        ASSERT_TRUE(instance->Play(Target, 0.5f, false, Inertialization));
    }
    ASSERT_TRUE(single.Update(0.25f));
    for (int i = 0; i < 5; ++i) ASSERT_TRUE(subdivided.Update(0.05f));
    EXPECT_NEAR(single.GetLocalPose()[0].Translation.x,
        subdivided.GetLocalPose()[0].Translation.x, 1e-5f);
    EXPECT_NEAR(single.GetCurveValues().GetValue("Value"),
        subdivided.GetCurveValues().GetValue("Value"), 1e-5f);
}

TEST_F(AnimationInertializationTest, CompletedStationarySourceDoesNotReuseStaleVelocity)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Moving, false));
    ASSERT_TRUE(instance.Update(5));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(instance.IsPlaying());
    ASSERT_TRUE(instance.Play(Clip(0), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 10, 1e-5f);
}

TEST_F(AnimationInertializationTest, DestinationProbeStaysInsideShortCycles)
{
    const auto shortClip = AnimationSequence::Create("Short", Mesh->GetSkeleton(), 0.003f,
        {{.BoneIndex = 0, .Translations = {{0, {1, 0, 0}}, {0.003f, {1.3f, 0, 0}}}}});
    ASSERT_TRUE(shortClip);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Clip(0)));
    ASSERT_TRUE(instance.Play(shortClip, 0.1f, true, Inertialization));
    ASSERT_TRUE(instance.Update(0.000001f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 0, 1e-6f);
}

TEST_F(AnimationInertializationTest, CrossFadeCanEnterAndLeaveAnInertialTransition)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Clip(0)));
    ASSERT_TRUE(instance.Play(Clip(8), 1.0f));
    ASSERT_TRUE(instance.Update(0.2f));
    const auto previous = instance.GetLocalPose()[0].Translation.x;
    ASSERT_TRUE(instance.Update(0.001f));
    const auto current = instance.GetLocalPose()[0].Translation.x;
    const auto velocity = (current - previous) / 0.001f;
    ASSERT_TRUE(instance.Play(Target, 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR((instance.GetLocalPose()[0].Translation.x - current) / 0.0001f,
        velocity, 0.02f);
    const auto smoothed = instance.GetLocalPose()[0].Translation.x;
    ASSERT_TRUE(instance.Play(Clip(0), 0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, smoothed);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, smoothed * 0.5f, 1e-5f);
}

TEST_F(AnimationInertializationTest, OverflowRejectsFrameBeforeNarrowingDoubleOffsetsToFloat)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Clip(0, 1e30f), false));
    ASSERT_TRUE(instance.Update(1));
    ASSERT_TRUE(instance.Play(Clip(0), std::numeric_limits<float>::max(), false, Inertialization));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Update(1e38f));
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    ASSERT_TRUE(instance.Update(0.01f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.01f);
}
