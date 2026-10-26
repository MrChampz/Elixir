#include <Engine/Animation/Pose.h>
#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationTestPlayback.h"
#include <Engine/Animation/BlendSpace1D.h>
#include <gtest/gtest.h>

#include "AnimationGraphTestDriver.h"
#include "../Mesh/SkinningTestData.h"

#include <limits>

using namespace Elixir;

namespace
{
    class BlendSpace1DTest : public testing::Test
    {
    protected:
        /** @brief Create clips whose differing cycle lengths expose phase mismatches. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            const auto skeleton = Mesh->GetSkeleton();
            Idle = AnimationSequence::Create("Idle", skeleton, 0, {});
            Walk = AnimationSequence::Create("Walk", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 0, 0, 0 } }, { 2, { 4, 0, 0 } }} },
            });
            Run = AnimationSequence::Create("Run", skeleton, 4, {
                { .BoneIndex = 0, .Translations = {{ 0, { 10, 0, 0 } }, { 4, { 18, 0, 0 } }} },
            });
            Space = BlendSpace1D::Create("Locomotion", {{ 6, Run }, { 0, Idle }, { 2, Walk }});
            ASSERT_TRUE(Space);
        }

        /** @brief Define a locomotion state with a shared numeric speed parameter. */
        SAnimationGraphTestInfo MakeController() const
        {
            return {
                .Name = "Controller",
                .Parameters = {{ "Speed", 4.0f }},
                .Layers = {{ .Node = MakeNode(AnimationStateMachine::Create({
                    .InitialState = "Locomotion",
                    .States = {{ .Name = "Locomotion", .Node = MakeNode(Space),
                    .InputParameters = { "Speed" } }}
                })) }},
            };
        }

        Ref<SkeletalMesh> Mesh;
        Ref<AnimationSequence> Idle;
        Ref<AnimationSequence> Walk;
        Ref<AnimationSequence> Run;
        Ref<BlendSpace1D> Space;
    };
}

TEST_F(BlendSpace1DTest, SortsSamplesAndInterpolatesNeighborWeightsAndCycleDurations)
{
    EXPECT_EQ(Space->GetName(), "Locomotion");
    EXPECT_EQ(Space->GetSkeleton(), Mesh->GetSkeleton());
    ASSERT_EQ(Space->GetSamples().size(), 3);
    EXPECT_FLOAT_EQ(Space->GetSamples()[0].Position, 0.0f);
    EXPECT_FLOAT_EQ(Space->GetSamples()[1].Position, 2.0f);
    EXPECT_FLOAT_EQ(Space->GetDuration(4.0f), 3.0f);
    EXPECT_FLOAT_EQ(Space->GetDuration(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(Space->GetDuration(-10.0f), 0.0f);
    EXPECT_FLOAT_EQ(Space->GetDuration(10.0f), 4.0f);
    SLocalPose pose;
    ASSERT_TRUE(Space->Sample(4.0f, 0.25f, pose));
    // Walk at 0.5 seconds gives x=1; Run at 1 second gives x=12.
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 6.5f);
    ASSERT_TRUE(Space->Sample(3.0f, 0.25f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 3.75f);
    ASSERT_TRUE(Space->Sample(1.0f, 0.25f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 0.5f);
}

TEST_F(BlendSpace1DTest, ClampsCoordinatesAndPhaseAndSupportsSingleSampleAssets)
{
    SLocalPose pose;
    ASSERT_TRUE(Space->Sample(-100.0f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 0.0f);
    ASSERT_TRUE(Space->Sample(100.0f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 14.0f);
    ASSERT_TRUE(Space->Sample(2.0f, -1.0f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 0.0f);
    ASSERT_TRUE(Space->Sample(2.0f, 5.0f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 4.0f);
    const auto single = BlendSpace1D::Create("Single", {{ 5.0f, Run }});
    ASSERT_TRUE(single);
    ASSERT_TRUE(single->Sample(-20.0f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 14.0f);
}

TEST_F(BlendSpace1DTest, BlendsLocalScaleAndShortestPathRotationWithMeshDefaults)
{
    const auto rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0, 0, 1));
    const auto turn = AnimationSequence::Create("Turn", Mesh->GetSkeleton(), 1, {
        { .BoneIndex = 0, .Rotations = {{ 0, -rotation }}, .Scales = {{ 0, { 3, 3, 3 } }} },
    });
    const auto space = BlendSpace1D::Create("Turn", {{ 0, Idle }, { 1, turn }});
    auto reference = Mesh->GetData().ReferencePose;
    reference.LocalTransforms[1].Translation = { 0, 7, 0 };
    SLocalPose pose;
    ASSERT_TRUE(space->Sample(0.5f, 0.5f, pose, reference));
    EXPECT_EQ(pose.LocalTransforms[1].Translation, glm::vec3(0, 7, 0));
    EXPECT_EQ(pose.LocalTransforms[0].Scale, glm::vec3(2));
    const auto expected = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1));
    EXPECT_NEAR(std::abs(glm::dot(pose.LocalTransforms[0].Rotation, expected)), 1.0f, 1e-5f);
}

TEST_F(BlendSpace1DTest, InvalidSamplingAndSingularScaleLeaveOutputUnchanged)
{
    SLocalPose pose = Mesh->GetData().ReferencePose;
    pose.LocalTransforms[0].Translation = { 99, 0, 0 };
    EXPECT_FALSE(Space->Sample(std::numeric_limits<float>::infinity(), 0.0f, pose));
    EXPECT_FALSE(Space->Sample(0.0f, std::numeric_limits<float>::quiet_NaN(), pose));
    EXPECT_FALSE(Space->Sample(0.0f, 0.0f, pose, SLocalPose{{pose.LocalTransforms.front()}}));
    const auto mirror = AnimationSequence::Create("Mirror", Mesh->GetSkeleton(), 0, {
        { .BoneIndex = 0, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    const auto space = BlendSpace1D::Create("Mirror", {{ 0, Idle }, { 1, mirror }});
    EXPECT_FALSE(space->Sample(0.5f, 0.0f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 99.0f);
}

TEST_F(BlendSpace1DTest, RejectsMissingIncompatibleDuplicateAndNonfiniteSamples)
{
    EXPECT_FALSE(BlendSpace1D::Create("Empty", {}));
    EXPECT_FALSE(BlendSpace1D::Create("", {{ 0, Idle }}));
    EXPECT_FALSE(BlendSpace1D::Create("Missing", {{ 0, nullptr }}));
    EXPECT_FALSE(BlendSpace1D::Create("Duplicate", {{ 0, Idle }, { 0, Walk }}));
    EXPECT_FALSE(BlendSpace1D::Create("NaN", {
        { std::numeric_limits<float>::quiet_NaN(), Idle },
    }));
    const auto incompatible = AnimationSequence::Create("Other",
        Skeleton::Create("Other", {{ .Name = "Other" }}), 0, {});
    EXPECT_FALSE(BlendSpace1D::Create("Incompatible", {{ 0, Idle }, { 1, incompatible }}));
    // Wide finite axis ranges must not overflow during interpolation.
    const auto maximum = std::numeric_limits<float>::max();
    const auto wide = BlendSpace1D::Create("Wide", {{ -maximum, Walk }, { maximum, Run }});
    ASSERT_TRUE(wide);
    SLocalPose pose;
    ASSERT_TRUE(wide->Sample(0.0f, 0.25f, pose));
    EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 6.5f);
}

TEST_F(BlendSpace1DTest, CoordinateChangesPreservePhaseAndUseTheNewCycleDuration)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }));
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 6.5f);
    ASSERT_TRUE(instance.SetInputs({ 6.0f }));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetTime(), 1.0f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 12.0f);
    ASSERT_TRUE(instance.Update(1.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 14.0f);
}

TEST_F(BlendSpace1DTest, StaticSamplesKeepPlaybackResponsiveAndFreezeTheCyclePhase)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 0.0f }));
    EXPECT_TRUE(instance.IsPlaying());
    ASSERT_TRUE(instance.Update(100.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.0f);
    ASSERT_TRUE(instance.SetInputs({ 2.0f }));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.SetInputs({ 0.0f }));
    ASSERT_TRUE(instance.Update(100.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.SetInputs({ 2.0f }));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 1.0f);
}

TEST_F(BlendSpace1DTest, SupportsPauseSeekLoopingAndNonloopingCompletion)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }));
    ASSERT_TRUE(instance.Update(0.75f));
    instance.Pause();
    ASSERT_TRUE(instance.Update(3.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    instance.Resume();
    ASSERT_TRUE(instance.Update(3.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.Seek(-0.75f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.75f);
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }, 0.0f, false));
    ASSERT_TRUE(instance.Update(4.0f));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 1.0f);
    EXPECT_FLOAT_EQ(instance.GetTime(), 3.0f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 11.0f);
    instance.Stop();
    EXPECT_FALSE(instance.SetInputs({ 4.0f }));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.0f);
}

TEST_F(BlendSpace1DTest, SequenceToBlendSpaceFadesAdvanceBothSourcesAndCanBeInterrupted)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Walk)));
    ASSERT_TRUE(instance.Seek(0.5f));
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }, 2.0f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 1.0f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 3.0f, 1e-5f);
    ASSERT_TRUE(instance.Play(MakeNode(Run), 1.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 3.0f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 7.0f, 1e-5f);
}

TEST_F(BlendSpace1DTest, BlendSpaceToSequenceAndBlendSpaceFadesRetainTheOutgoingCycle)
{
    for (const bool targetSpace : { false, true })
    {
        const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
        ASSERT_TRUE(instanceHandle);
        auto& instance = *instanceHandle;
        ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }));
        ASSERT_TRUE(instance.Update(0.75f));
        if (targetSpace)
            ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 6.0f }, 2.0f));
        else
            ASSERT_TRUE(instance.Play(MakeNode(Run), 2.0f));
        ASSERT_TRUE(instance.Update(0.5f));
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 8.375f, 1e-5f);
        ASSERT_TRUE(instance.Update(1.5f));
        EXPECT_FALSE(instance.IsTransitioning());
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 14.0f, 1e-5f);
    }
}

TEST_F(BlendSpace1DTest, InvalidPlaybackRequestsPreserveTheCurrentPoseAndPhase)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }));
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_FALSE(PlayWithInputs(instance, MakeNode(nullptr), { 4.0f }));
    EXPECT_FALSE(PlayWithInputs(instance, MakeNode(Space), { std::numeric_limits<float>::infinity() }));
    EXPECT_FALSE(PlayWithInputs(instance, MakeNode(Space), { 4.0f }, -1.0f));
    EXPECT_FALSE(instance.SetInputs({ std::numeric_limits<float>::quiet_NaN() }));
    EXPECT_FALSE(instance.Update(-1.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 6.5f);
    EXPECT_FALSE(AnimationTestCharacter::Create(nullptr));
}

TEST_F(BlendSpace1DTest, ControllerEvaluatesContinuousCoordinatesWithoutRestartingTheState)
{
    const auto controller = CreateTestGraph(MakeController());
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_EQ(instance.GetCurrentState(), "Locomotion");
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 6.5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 6.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_FLOAT_EQ(mesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 12.0f);
    EXPECT_FALSE(mesh.IsTransitioning());
}

TEST_F(BlendSpace1DTest, ControllerTransitionsBetweenBlendSpaceAndSequenceStates)
{
    auto info = MakeController();
    info.Parameters.push_back({ "Attack", false });
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States.push_back({ .Name = "Attack", .Node = MakeNode(Run), .Looping = false });
        machine.Transitions = {
        { .From = "Locomotion", .To = "Attack",
          .Conditions = {{ "Attack", EAnimationComparison::Equal, true }},
          .FadeDuration = 0.5f },
        { .From = "Attack", .To = "Locomotion", .FadeDuration = 0.5f,
          .WaitForCompletion = true },
    };
    });
    const auto controller = CreateTestGraph(std::move(info));
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.75f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Attack");
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 6.5f);
    ASSERT_TRUE(instance.SetBool("Attack", false));
    ASSERT_TRUE(instance.Update(4.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Locomotion");
    EXPECT_TRUE(mesh.IsTransitioning());
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(mesh.IsTransitioning());
}

TEST_F(BlendSpace1DTest, ControllerWaitsForNonloopingBlendSpaceCompletion)
{
    auto info = MakeController();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[0].Looping = false;
        machine.States.push_back({ "Idle", MakeNode(Idle) });
        machine.Transitions = {{ .From = "Locomotion", .To = "Idle", .WaitForCompletion = true }};
    });
    const auto controller = CreateTestGraph(std::move(info));
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(3.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Locomotion");
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
}

TEST_F(BlendSpace1DTest, ControllerRejectsMissingWronglyTypedAndAmbiguousSources)
{
    auto info = MakeController();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[0].InputParameters = { "Missing" };
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeController();
    info.Parameters[0].DefaultValue = true;
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeController();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[0].Node = MakeNode(Walk);
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeController();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[0].Node.reset();
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeController();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States.push_back({ .Name = "Other", .Node = MakeNode(Run), .InputParameters = { "Speed" } });
    });
    EXPECT_FALSE(CreateTestGraph(info));
}

TEST_F(BlendSpace1DTest, CharactersShareTheSpaceWithIndependentCoordinatesAndCyclePhases)
{
    const auto controller = CreateTestGraph(MakeController());
    const auto firstMeshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(firstMeshHandle);
    auto& firstMesh = *firstMeshHandle;
    const auto secondMeshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(secondMeshHandle);
    auto& secondMesh = *secondMeshHandle;
    AnimationGraphTestDriver first(controller, firstMesh), second(controller, secondMesh);
    ASSERT_TRUE(first.SetFloat("Speed", 2.0f));
    ASSERT_TRUE(second.SetFloat("Speed", 6.0f));
    ASSERT_TRUE(first.Update(0.5f));
    ASSERT_TRUE(second.Update(2.0f));
    EXPECT_FLOAT_EQ(firstMesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(secondMesh.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(firstMesh.GetLocalPose().LocalTransforms[0].Translation.x, 1.0f);
    EXPECT_FLOAT_EQ(secondMesh.GetLocalPose().LocalTransforms[0].Translation.x, 14.0f);
    ASSERT_TRUE(first.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(first.Update(0.0f));
    EXPECT_FLOAT_EQ(firstMesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(secondMesh.GetLocalPose().LocalTransforms[0].Translation.x, 14.0f);
}
