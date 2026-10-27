#include <Engine/Animation/BlendSpace1D.h>
#include <gtest/gtest.h>

#include <Engine/Animation/AnimationControllerInstance.h>
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
        SAnimationControllerCreateInfo MakeController() const
        {
            return {
                .Name = "Controller", .InitialState = "Locomotion",
                .Parameters = {{ "Speed", 4.0f }},
                .States = {{ .Name = "Locomotion", .BlendSpace = Space,
                    .BlendParameters = { "Speed" } }},
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
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(Space->Sample(4.0f, 0.25f, pose));
    // Walk at 0.5 seconds gives x=1; Run at 1 second gives x=12.
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 6.5f);
    ASSERT_TRUE(Space->Sample(3.0f, 0.25f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 3.75f);
    ASSERT_TRUE(Space->Sample(1.0f, 0.25f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 0.5f);
}

TEST_F(BlendSpace1DTest, ClampsCoordinatesAndPhaseAndSupportsSingleSampleAssets)
{
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(Space->Sample(-100.0f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 0.0f);
    ASSERT_TRUE(Space->Sample(100.0f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 14.0f);
    ASSERT_TRUE(Space->Sample(2.0f, -1.0f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 0.0f);
    ASSERT_TRUE(Space->Sample(2.0f, 5.0f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 4.0f);
    const auto single = BlendSpace1D::Create("Single", {{ 5.0f, Run }});
    ASSERT_TRUE(single);
    ASSERT_TRUE(single->Sample(-20.0f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 14.0f);
}

TEST_F(BlendSpace1DTest, BlendsLocalScaleAndShortestPathRotationWithMeshDefaults)
{
    const auto rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0, 0, 1));
    const auto turn = AnimationSequence::Create("Turn", Mesh->GetSkeleton(), 1, {
        { .BoneIndex = 0, .Rotations = {{ 0, -rotation }}, .Scales = {{ 0, { 3, 3, 3 } }} },
    });
    const auto space = BlendSpace1D::Create("Turn", {{ 0, Idle }, { 1, turn }});
    auto reference = Mesh->GetData().ReferencePose;
    reference[1].Translation = { 0, 7, 0 };
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(space->Sample(0.5f, 0.5f, pose, reference));
    EXPECT_EQ(pose[1].Translation, glm::vec3(0, 7, 0));
    EXPECT_EQ(pose[0].Scale, glm::vec3(2));
    const auto expected = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1));
    EXPECT_NEAR(std::abs(glm::dot(pose[0].Rotation, expected)), 1.0f, 1e-5f);
}

TEST_F(BlendSpace1DTest, InvalidSamplingAndSingularScaleLeaveOutputUnchanged)
{
    std::vector<SBoneTransform> pose = Mesh->GetData().ReferencePose;
    pose[0].Translation = { 99, 0, 0 };
    EXPECT_FALSE(Space->Sample(std::numeric_limits<float>::infinity(), 0.0f, pose));
    EXPECT_FALSE(Space->Sample(0.0f, std::numeric_limits<float>::quiet_NaN(), pose));
    EXPECT_FALSE(Space->Sample(0.0f, 0.0f, pose, std::span(pose).first(1)));
    const auto mirror = AnimationSequence::Create("Mirror", Mesh->GetSkeleton(), 0, {
        { .BoneIndex = 0, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    const auto space = BlendSpace1D::Create("Mirror", {{ 0, Idle }, { 1, mirror }});
    EXPECT_FALSE(space->Sample(0.5f, 0.0f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 99.0f);
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
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(wide->Sample(0.0f, 0.25f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 6.5f);
}

TEST_F(BlendSpace1DTest, CoordinateChangesPreservePhaseAndUseTheNewCycleDuration)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Space, { 4.0f }));
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 6.5f);
    ASSERT_TRUE(instance.SetBlendSpaceValue({ 6.0f }));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetTime(), 1.0f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 12.0f);
    ASSERT_TRUE(instance.Update(1.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 14.0f);
}

TEST_F(BlendSpace1DTest, StaticSamplesKeepPlaybackResponsiveAndFreezeTheCyclePhase)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Space, { 0.0f }));
    EXPECT_TRUE(instance.IsPlaying());
    ASSERT_TRUE(instance.Update(100.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.0f);
    ASSERT_TRUE(instance.SetBlendSpaceValue({ 2.0f }));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.SetBlendSpaceValue({ 0.0f }));
    ASSERT_TRUE(instance.Update(100.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.SetBlendSpaceValue({ 2.0f }));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 1.0f);
}

TEST_F(BlendSpace1DTest, SupportsPauseSeekLoopingAndNonloopingCompletion)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Space, { 4.0f }));
    ASSERT_TRUE(instance.Update(0.75f));
    instance.Pause();
    ASSERT_TRUE(instance.Update(3.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    instance.Resume();
    ASSERT_TRUE(instance.Update(3.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.Seek(-0.75f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.75f);
    ASSERT_TRUE(instance.Play(Space, { 4.0f }, 0.0f, false));
    ASSERT_TRUE(instance.Update(4.0f));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 1.0f);
    EXPECT_FLOAT_EQ(instance.GetTime(), 3.0f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 11.0f);
    instance.Stop();
    EXPECT_FALSE(instance.SetBlendSpaceValue({ 4.0f }));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.0f);
}

TEST_F(BlendSpace1DTest, SequenceToBlendSpaceFadesAdvanceBothSourcesAndCanBeInterrupted)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Walk));
    ASSERT_TRUE(instance.Seek(0.5f));
    ASSERT_TRUE(instance.Play(Space, { 4.0f }, 2.0f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 1.0f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 3.0f, 1e-5f);
    ASSERT_TRUE(instance.Play(Run, 1.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 3.0f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 7.0f, 1e-5f);
}

TEST_F(BlendSpace1DTest, BlendSpaceToSequenceAndBlendSpaceFadesRetainTheOutgoingCycle)
{
    for (const bool targetSpace : { false, true })
    {
        SkeletalMeshInstance instance(Mesh);
        ASSERT_TRUE(instance.Play(Space, { 4.0f }));
        ASSERT_TRUE(instance.Update(0.75f));
        if (targetSpace)
            ASSERT_TRUE(instance.Play(Space, { 6.0f }, 2.0f));
        else
            ASSERT_TRUE(instance.Play(Run, 2.0f));
        ASSERT_TRUE(instance.Update(0.5f));
        EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 8.375f, 1e-5f);
        ASSERT_TRUE(instance.Update(1.5f));
        EXPECT_FALSE(instance.IsTransitioning());
        EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 14.0f, 1e-5f);
    }
}

TEST_F(BlendSpace1DTest, InvalidPlaybackRequestsPreserveTheCurrentPoseAndPhase)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Space, { 4.0f }));
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_FALSE(instance.Play(nullptr, { 4.0f }));
    EXPECT_FALSE(instance.Play(Space, { std::numeric_limits<float>::infinity() }));
    EXPECT_FALSE(instance.Play(Space, { 4.0f }, -1.0f));
    EXPECT_FALSE(instance.SetBlendSpaceValue({ std::numeric_limits<float>::quiet_NaN() }));
    EXPECT_FALSE(instance.Update(-1.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 6.5f);
    SkeletalMeshInstance invalid(nullptr);
    EXPECT_FALSE(invalid.Play(Space, { 4.0f }));
}

TEST_F(BlendSpace1DTest, ControllerEvaluatesContinuousCoordinatesWithoutRestartingTheState)
{
    const auto controller = AnimationController::Create(MakeController());
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_EQ(instance.GetCurrentState(), "Locomotion");
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 6.5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 6.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_FLOAT_EQ(mesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 12.0f);
    EXPECT_FALSE(mesh.IsTransitioning());
}

TEST_F(BlendSpace1DTest, ControllerTransitionsBetweenBlendSpaceAndSequenceStates)
{
    auto info = MakeController();
    info.Parameters.push_back({ "Attack", false });
    info.States.push_back({ .Name = "Attack", .Sequence = Run, .Looping = false });
    info.Transitions = {
        { .From = "Locomotion", .To = "Attack",
          .Conditions = {{ "Attack", EAnimationComparison::Equal, true }},
          .FadeDuration = 0.5f },
        { .From = "Attack", .To = "Locomotion", .FadeDuration = 0.5f,
          .WaitForCompletion = true },
    };
    const auto controller = AnimationController::Create(std::move(info));
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.75f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Attack");
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 6.5f);
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
    info.States[0].Looping = false;
    info.States.push_back({ "Idle", Idle });
    info.Transitions = {{ .From = "Locomotion", .To = "Idle", .WaitForCompletion = true }};
    const auto controller = AnimationController::Create(std::move(info));
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(3.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Locomotion");
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
}

TEST_F(BlendSpace1DTest, ControllerRejectsMissingWronglyTypedAndAmbiguousSources)
{
    auto info = MakeController();
    info.States[0].BlendParameters = { "Missing" };
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeController();
    info.Parameters[0].DefaultValue = true;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeController();
    info.States[0].Sequence = Walk;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeController();
    info.States[0].BlendSpace.reset();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeController();
    info.States.push_back({ .Name = "Other", .Sequence = Run, .BlendParameters = { "Speed" } });
    EXPECT_FALSE(AnimationController::Create(info));
}

TEST_F(BlendSpace1DTest, CharactersShareTheSpaceWithIndependentCoordinatesAndCyclePhases)
{
    const auto controller = AnimationController::Create(MakeController());
    SkeletalMeshInstance firstMesh(Mesh), secondMesh(Mesh);
    AnimationControllerInstance first(controller, firstMesh), second(controller, secondMesh);
    ASSERT_TRUE(first.SetFloat("Speed", 2.0f));
    ASSERT_TRUE(second.SetFloat("Speed", 6.0f));
    ASSERT_TRUE(first.Update(0.5f));
    ASSERT_TRUE(second.Update(2.0f));
    EXPECT_FLOAT_EQ(firstMesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(secondMesh.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(firstMesh.GetLocalPose()[0].Translation.x, 1.0f);
    EXPECT_FLOAT_EQ(secondMesh.GetLocalPose()[0].Translation.x, 14.0f);
    ASSERT_TRUE(first.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(first.Update(0.0f));
    EXPECT_FLOAT_EQ(firstMesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(secondMesh.GetLocalPose()[0].Translation.x, 14.0f);
}
