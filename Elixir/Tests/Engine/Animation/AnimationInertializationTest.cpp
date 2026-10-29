#include "AnimationInertializationTestFixture.h"
#include "AnimationTestNodes.h"
#include <Engine/Animation/BlendSpace1D.h>
#include <limits>

using namespace Elixir;
using namespace Elixir::Tests::Inertia;
using AnimationInertializationTest = AnimationInertializationFixture;

namespace
{
    /**
     * @brief Read a shortest angular displacement in radians.
     * @param rotation Relative unit quaternion.
     */
    glm::vec3 AngularDisplacement(glm::quat rotation)
    {
        if (rotation.w < 0)
            rotation = -rotation;
        const glm::vec3 axis(rotation.x, rotation.y, rotation.z);
        const auto length = glm::length(axis);
        return length < 1e-7f ? axis * 2.0f
                              : axis * (2.0f * std::atan2(length, rotation.w) / length);
    }

}

TEST_F(AnimationInertializationTest, PreservesPoseAndOutgoingVelocityAtDestinationChange)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving), false));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto position = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    ASSERT_TRUE(instance.Play(MakeNode(Target), 1.0f, false, Inertialization));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, position);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, position);
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(
        (instance.GetLocalPose().LocalTransforms[0].Translation.x - position) / 0.0001f,
        2,
        0.01f
    );
    EXPECT_NEAR(instance.GetCurveValues().GetValue("Value"), position + 0.0002f, 1e-5f);
}

TEST_F(
    AnimationInertializationTest,
    InterruptedTransitionKeepsRecentVelocityInsteadOfFreezingSource
)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(Target), 1.0f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.2f));
    const auto previous = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    ASSERT_TRUE(instance.Update(0.001f));
    const auto current = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    const auto speed = (current - previous) / 0.001f;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(-5, 1)), 0.5f, false, Inertialization));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, current);
    ASSERT_TRUE(instance.Update(0.0001f));
    const auto next = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    EXPECT_NEAR((next - current) / 0.0001f, speed, 0.02f);
}

TEST_F(AnimationInertializationTest, ConvergesToDestinationPoseCurvesAndVelocityAtDuration)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Moving)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(Target), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.4999f));
    const auto previous = instance.GetLocalPose().LocalTransforms[0].Translation.x;
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 8.5f, 1e-5f);
    EXPECT_NEAR(instance.GetCurveValues().GetValue("Value"), 8.5f, 1e-5f);
    EXPECT_NEAR(
        (instance.GetLocalPose().LocalTransforms[0].Translation.x - previous) / 0.0001f,
        -3,
        0.02f
    );
    EXPECT_FALSE(instance.GetBaseLayer().GetPlayer().IsTransitioning());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 7.75f, 1e-5f);
}

TEST_F(AnimationInertializationTest, PreservesNoncommutingAngularVelocityAndUnitRotations)
{
    const auto source = Sequence::Create(
        "Source",
        Mesh->GetSkeleton(),
        5,
        {{.BoneIndex = 0,
          .Rotations = {
              {0, glm::quat(1, 0, 0, 0)}, {5, glm::angleAxis(2.5f, glm::vec3(0, 0, 1))}
          }}}
    );
    const auto targetStart = glm::angleAxis(1.4f, glm::normalize(glm::vec3(1, 2, 0)));
    const auto target = Sequence::Create(
        "Target",
        Mesh->GetSkeleton(),
        5,
        {{.BoneIndex = 0,
          .Rotations = {
              {0, targetStart}, {5, glm::angleAxis(1.0f, glm::vec3(0, 1, 0)) * targetStart}
          }}}
    );
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(source)));
    ASSERT_TRUE(instance.Update(0.1f));
    const auto rotation = instance.GetLocalPose().LocalTransforms[0].Rotation;
    ASSERT_TRUE(instance.Play(MakeNode(target), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    const auto velocity =
        AngularDisplacement(
            instance.GetLocalPose().LocalTransforms[0].Rotation * glm::conjugate(rotation)
        ) /
        0.0001f;
    EXPECT_NEAR(velocity.x, 0, 0.005f);
    EXPECT_NEAR(velocity.y, 0, 0.005f);
    EXPECT_NEAR(velocity.z, 0.5f, 0.005f);
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(instance.Update(0.05f));
        EXPECT_TRUE(instance.GetLocalPose().LocalTransforms[0].IsValid());
    }
}

TEST_F(AnimationInertializationTest, QuaternionSignsAndHalfTurnsKeepShortestFinitePaths)
{
    const auto halfTurn = glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0));
    const auto first = Sequence::Create(
        "First",
        Mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Rotations = {{0, halfTurn}}}}
    );
    const auto second = Sequence::Create(
        "Second",
        Mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Rotations = {{0, -halfTurn}}}}
    );
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(first)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_TRUE(instance.Play(MakeNode(second), 0.2f, true, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_NEAR(
        std::abs(glm::dot(instance.GetLocalPose().LocalTransforms[0].Rotation, halfTurn)),
        1,
        1e-5f
    );
    ASSERT_TRUE(instance.Play(MakeNode(Moving), 0.2f, true, Inertialization));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(instance.GetLocalPose().LocalTransforms[0].IsValid());
}

TEST_F(AnimationInertializationTest, SignedLogarithmicScalePreservesReflectionsAndAvoidsZero)
{
    const auto source = Sequence::Create(
        "Source",
        Mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Scales = {{0, {-2, 2, 2}}}}}
    );
    const auto target = Sequence::Create(
        "Target",
        Mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Scales = {{0, {-8, 8, 8}}}}}
    );
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(source)));
    ASSERT_TRUE(instance.Play(MakeNode(target), 1.0f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Scale.x, -4, 1e-5f);
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Scale.y, 4, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Scale.x, -8);
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Play(MakeNode(Moving), 0.5f, false, Inertialization));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    // An immediate switch intentionally accepts reflection changes.
    ASSERT_TRUE(instance.Play(MakeNode(Moving), 0.0f, false, Inertialization));
}

TEST_F(
    AnimationInertializationTest,
    MissingHistoryUsesZeroOutgoingVelocityAndMissingCurvesUseZero
)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(2))));
    ASSERT_TRUE(instance.Play(MakeNode(Target->WithCurves({})), 0.5f, false, Inertialization));
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 2, 1e-5f);
    EXPECT_NEAR(instance.GetCurveValues().GetValue("Value"), 2, 1e-5f);
    ASSERT_TRUE(instance.Update(0.4999f));
    EXPECT_FLOAT_EQ(instance.GetCurveValues().GetValue("Value"), 0);
    EXPECT_TRUE(instance.GetCurveValues().GetValues().empty());
}

TEST_F(AnimationInertializationTest, CurvesOnlyInPreviousSamplePreserveOutgoingVelocity)
{
    const auto withoutCurves = Clip(0)->WithCurves({});
    const auto space = BlendSpace1D::Create("Space", {{0, Clip(2)}, {1, withoutCurves}});
    ASSERT_TRUE(space);
    auto& player = Playback->GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.Play(MakeNode(space), {0}));
    ASSERT_TRUE(Playback->Update(0.1f));
    ASSERT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Value"), 2);

    ASSERT_TRUE(player.SetSourceInputs({1}));
    ASSERT_TRUE(Playback->Update(0.1f));
    ASSERT_TRUE(Playback->GetCurveValues().GetValues().empty());
    ASSERT_TRUE(Playback->Play(MakeNode(withoutCurves), 0.5f, false, Inertialization));
    ASSERT_TRUE(Playback->Update(0));
    EXPECT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Value"), 0);

    ASSERT_TRUE(Playback->Update(0.0001f));
    EXPECT_NEAR(Playback->GetCurveValues().GetValue("Value") / 0.0001f, -20, 0.01f);
    ASSERT_TRUE(Playback->Update(0.2499f));
    EXPECT_NEAR(Playback->GetCurveValues().GetValue("Value"), -1.5625f, 1e-5f);
    ASSERT_TRUE(Playback->Update(0.25f));
    EXPECT_FLOAT_EQ(Playback->GetCurveValues().GetValue("Value"), 0);
    EXPECT_TRUE(Playback->GetCurveValues().GetValues().empty());
}

TEST_F(AnimationInertializationTest, FrameSubdivisionDoesNotChangeCapturedOffsetDecay)
{
    auto& single = *Playback;
    const auto subdividedHandle = CreatePlayback();
    ASSERT_TRUE(subdividedHandle);
    auto& subdivided = *subdividedHandle;
    for (auto* instance : {&single, &subdivided})
    {
        ASSERT_TRUE(instance->Play(MakeNode(Moving)));
        ASSERT_TRUE(instance->Update(0.1f));
        ASSERT_TRUE(instance->Play(MakeNode(Target), 0.5f, false, Inertialization));
    }
    ASSERT_TRUE(single.Update(0.25f));
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(subdivided.Update(0.05f));
    EXPECT_NEAR(
        single.GetLocalPose().LocalTransforms[0].Translation.x,
        subdivided.GetLocalPose().LocalTransforms[0].Translation.x,
        1e-5f
    );
    EXPECT_NEAR(
        single.GetCurveValues().GetValue("Value"),
        subdivided.GetCurveValues().GetValue("Value"),
        1e-5f
    );
}

TEST_F(AnimationInertializationTest, DestinationProbeStaysInsideShortCycles)
{
    const auto shortClip = Sequence::Create(
        "Short",
        Mesh->GetSkeleton(),
        0.003f,
        {{.BoneIndex = 0, .Translations = {{0, {1, 0, 0}}, {0.003f, {1.3f, 0, 0}}}}}
    );
    ASSERT_TRUE(shortClip);
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(0))));
    ASSERT_TRUE(instance.Play(MakeNode(shortClip), 0.1f, true, Inertialization));
    ASSERT_TRUE(instance.Update(0.000001f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0, 1e-6f);
}

TEST_F(AnimationInertializationTest, OverflowRejectsFrameBeforeNarrowingDoubleOffsetsToFloat)
{
    auto& instance = *Playback;
    ASSERT_TRUE(instance.Play(MakeNode(Clip(0, 1e30f)), false));
    ASSERT_TRUE(instance.Update(1));
    ASSERT_TRUE(instance.Play(
        MakeNode(Clip(0)),
        std::numeric_limits<float>::max(),
        false,
        Inertialization
    ));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Update(1e38f));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    ASSERT_TRUE(instance.Update(0.01f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.01f);
}
