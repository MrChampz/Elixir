#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationTestPlayback.h"
#include <gtest/gtest.h>
#include <Engine/Animation/BlendSpace1D.h>
#include "AnimationGraphTestDriver.h"
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    class RootMotionTest : public testing::Test
    {
    protected:
        /** @brief Create a two-bone mesh for root-motion extraction. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
        }

        /**
         * @brief Create a one-second clip translating and optionally turning its root.
         * @param distance End translation along local X.
         * @param degrees End rotation around local Y.
         * @param offset Authored starting translation, independent of the reference pose.
         */
        Ref<AnimationSequence> MakeClip(float distance, float degrees = 0, float offset = 0)
        {
            return AnimationSequence::Create("Motion", Mesh->GetSkeleton(), 1, {{
                .BoneIndex = 0,
                .Translations = {{ 0, {offset, 0, 0} }, { 1, {offset + distance, 0, 0} }},
                .Rotations = {{ 0, {1, 0, 0, 0} },
                    { 1, glm::angleAxis(glm::radians(degrees), glm::vec3(0, 1, 0)) }},
            }});
        }

        /**
         * @brief Compare the matrices of two rigid movements.
         * @param a First movement.
         * @param b Second movement.
         */
        void ExpectNear(const SRootMotionDelta& a, const SRootMotionDelta& b)
        {
            const auto first = a.ToMatrix();
            const auto second = b.ToMatrix();
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    EXPECT_NEAR(first[column][row], second[column][row], 1e-4f);
        }

        Ref<SkeletalMesh> Mesh;
    };
}

TEST_F(RootMotionTest, DisabledByDefaultAndEnabledPoseMovesOnlyThroughTheExtractedDelta)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(2)), false));
    EXPECT_FALSE(mesh.IsRootMotionEnabled());
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 0);
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(mesh.Update(1));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 1);
    EXPECT_FLOAT_EQ(mesh.GetTime(), 1);
    ASSERT_TRUE(mesh.Update(1));
    ExpectNear(mesh.GetRootMotionDelta(), {});
}

TEST_F(RootMotionTest, LoopBoundaryAndMultipleCyclesPreserveForwardMovement)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(2))));
    ASSERT_TRUE(mesh.Update(0.75f));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 1.5f);
    ASSERT_TRUE(mesh.Update(0.5f));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 1);
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    ASSERT_TRUE(mesh.Update(3.5f));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 7);
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
}

TEST_F(RootMotionTest, TurningCyclesComposeRigidMovementAndAreIndependentOfFramePartition)
{
    const auto clip = MakeClip(1, 90);
    const auto& reference = Mesh->GetData().ReferencePose.LocalTransforms[0];
    SRootMotionDelta cycle, full, partitioned;
    ASSERT_TRUE(clip->ExtractRootMotion(0, 1, true, reference, cycle));
    ASSERT_TRUE(clip->ExtractRootMotion(0, 2, true, reference, full));
    ExpectNear(full, cycle * cycle);
    for (int i = 0; i < 16; ++i)
    {
        SRootMotionDelta part;
        ASSERT_TRUE(clip->ExtractRootMotion(i / 8.0, (i + 1) / 8.0, true, reference, part));
        partitioned = partitioned * part;
    }
    ExpectNear(full, partitioned);
    EXPECT_NEAR(full.Translation.x, 1, 1e-5f);
    EXPECT_NEAR(full.Translation.z, -1, 1e-5f);
}

TEST_F(RootMotionTest, ReferenceRootOrientationConvertsMovementToMeshSpace)
{
    auto reference = Mesh->GetData().ReferencePose.LocalTransforms[0];
    reference.Rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    SRootMotionDelta motion;
    ASSERT_TRUE(MakeClip(2)->ExtractRootMotion(0, 0.5, false, reference, motion));
    EXPECT_NEAR(motion.Translation.x, 0, 1e-5f);
    EXPECT_NEAR(motion.Translation.z, -1, 1e-5f);
}

TEST_F(RootMotionTest, SeekPauseZeroDeltaAndPlaybackChangesDoNotGenerateMovement)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(2))));
    ASSERT_TRUE(mesh.Update(0.25f));
    ASSERT_TRUE(mesh.Seek(0.75f));
    ExpectNear(mesh.GetRootMotionDelta(), {});
    ASSERT_TRUE(mesh.Update(0));
    ExpectNear(mesh.GetRootMotionDelta(), {});
    mesh.Pause();
    ASSERT_TRUE(mesh.Update(0.5f));
    ExpectNear(mesh.GetRootMotionDelta(), {});
    mesh.Resume();
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 0.5f);
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(3, 0, 100))));
    ExpectNear(mesh.GetRootMotionDelta(), {});
    ASSERT_TRUE(mesh.Update(0.1f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 0.3f, 1e-5f);
    mesh.Stop();
    ExpectNear(mesh.GetRootMotionDelta(), {});
}

TEST_F(RootMotionTest, FadeBlendsMovementRatherThanAbsoluteRootPositions)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(1))));
    ASSERT_TRUE(mesh.Update(0.25f));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(3, 0, 100)), 1.0f));
    ASSERT_TRUE(mesh.Update(0.5f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 0.75f, 1e-5f);
    ASSERT_TRUE(mesh.Update(0.5f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 1.25f, 1e-5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0);
}

TEST_F(RootMotionTest, FadeCompletionWithinALargeUpdateIncludesTheRemainingTargetMovement)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(1))));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(3)), 1.0f));
    ASSERT_TRUE(mesh.Update(2));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 5, 1e-5f);
}

TEST_F(RootMotionTest, InterruptedFadeUsesStationarySnapshotWithoutAnOffsetJump)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(1))));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(3)), 1.0f));
    ASSERT_TRUE(mesh.Update(0.25f));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(5, 0, 200)), 1.0f));
    ASSERT_TRUE(mesh.Update(0.5f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 0.625f, 1e-5f);
}

TEST_F(RootMotionTest, BlendSpaceWeightsBlendDeltasWithoutJumpsWhenCoordinatesChange)
{
    const auto space = BlendSpace1D::Create("Move", {{0, MakeClip(2)}, {1, MakeClip(4)}});
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(PlayWithInputs(mesh, MakeNode(space), {0.5f}));
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 0.75f, 1e-5f);
    ASSERT_TRUE(mesh.SetInputs({1}));
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 1, 1e-5f);
    ASSERT_TRUE(mesh.Update(0.75f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 3, 1e-5f);
}

TEST_F(RootMotionTest, MarkerMappingKeepsContinuousRootMovementAcrossTheClipBoundary)
{
    const auto first = MakeClip(1)->WithSyncMarkers({{"L", 0.25f}, {"R", 0.75f}});
    const auto second = MakeClip(2)->WithSyncMarkers({{"L", 0.1f}, {"R", 0.6f}});
    const auto space = BlendSpace1D::Create("Marked", {{0, first}, {1, second}});
    const auto& reference = Mesh->GetData().ReferencePose.LocalTransforms[0];
    SRootMotionDelta motion;
    ASSERT_TRUE(space->ExtractRootMotion(std::array{0.5f}, 0, 1, true, reference, motion));
    EXPECT_NEAR(motion.Translation.x, 1.5f, 1e-5f);
    ASSERT_TRUE(space->ExtractRootMotion(std::array{0.5f}, 0, 0.5, true, reference, motion));
    EXPECT_NEAR(motion.Translation.x, 0.75f, 1e-5f);
    for (int i = 0; i <= 20; ++i)
    {
        const auto phase = i / 20.0;
        EXPECT_NEAR(second->GetSynchronizedPhase(phase + 1, *first),
            second->GetSynchronizedPhase(phase, *first) + 1, 1e-6);
    }
}

TEST_F(RootMotionTest, InvalidOrExcessiveIntervalsPreserveOutputAndAllMeshClocks)
{
    const auto clip = MakeClip(1);
    const auto& reference = Mesh->GetData().ReferencePose.LocalTransforms[0];
    SRootMotionDelta output{glm::vec3(7, 0, 0)};
    EXPECT_FALSE(clip->ExtractRootMotion(1, 0, true, reference, output));
    EXPECT_FALSE(clip->ExtractRootMotion(0, std::numeric_limits<double>::infinity(),
        true, reference, output));
    EXPECT_FLOAT_EQ(output.Translation.x, 7);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(clip)));
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_FALSE(mesh.Update(4097));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0);
}

TEST_F(RootMotionTest, ControllerUpdatesRootMotionAndOtherLayersDoNotDriveTheCharacter)
{
    const auto controller = CreateTestGraph({
        .Name = "Root", .Layers = {
            {.Node = MakeNode(MakeClip(2))},
            {.Name = "UpperBody", .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
                .Node = MakeNode(MakeClip(100))},
        },
    });
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0);
}

TEST_F(RootMotionTest, MissingRootTrackStaticClipsAndInvalidMeshAreSafe)
{
    EXPECT_FALSE(AnimationTestCharacter::Create(nullptr));
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(AnimationSequence::Create("Empty", Mesh->GetSkeleton(), 1, {}))));
    ASSERT_TRUE(mesh.Update(0.5f));
    ExpectNear(mesh.GetRootMotionDelta(), {});
    ASSERT_TRUE(mesh.Play(MakeNode(AnimationSequence::Create("Static", Mesh->GetSkeleton(), 0, {}))));
    ASSERT_TRUE(mesh.Update(1));
    ExpectNear(mesh.GetRootMotionDelta(), {});
    ASSERT_TRUE(mesh.SetRootMotionEnabled(false));
    EXPECT_FALSE(mesh.IsRootMotionEnabled());
}

TEST_F(RootMotionTest, CyclicMarkerNamesPreserveUnwrappedPhaseAndMatchPoseSampling)
{
    const auto first = MakeClip(1)->WithSyncMarkers({{"L", 0.25f}, {"R", 0.75f}});
    const auto second = MakeClip(2)->WithSyncMarkers({{"R", 0.1f}, {"L", 0.7f}});
    for (int i = 0; i <= 100; ++i)
    {
        const auto phase = i / 100.0;
        const auto continuous = second->GetSynchronizedPhase(phase, *first);
        const auto wrapped = continuous - std::floor(continuous);
        const auto sampled = second->GetSynchronizedTime(float(phase), *first);
        EXPECT_NEAR(wrapped, sampled, 1e-5);
        EXPECT_NEAR(second->GetSynchronizedPhase(phase + 1, *first), continuous + 1, 1e-6);
    }
}

TEST_F(RootMotionTest, ManualPoseLocksTheRootAndClearsThePreviousMovement)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(2))));
    ASSERT_TRUE(mesh.Update(0.25f));
    auto pose = Mesh->GetData().ReferencePose;
    pose.LocalTransforms[0].Translation.x = 10;
    ASSERT_TRUE(mesh.SetLocalPose(pose));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    EXPECT_FLOAT_EQ(mesh.GetModelPose().Matrices[0][3].x, 0);
    ExpectNear(mesh.GetRootMotionDelta(), {});
    ASSERT_TRUE(mesh.Update(0.5f));
    ExpectNear(mesh.GetRootMotionDelta(), {});
}

TEST_F(RootMotionTest, FailedUpperLayerCompositionPreservesBaseClockAndPreviousMovement)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    ASSERT_TRUE(mesh.Play(MakeNode(MakeClip(1))));
    ASSERT_TRUE(mesh.Update(0.1f));
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    ASSERT_TRUE(mesh.AddLayer("Upper", mask));
    const auto mirror = AnimationSequence::Create("Mirror", Mesh->GetSkeleton(), 1, {
        {.BoneIndex = 1, .Scales = {{0, {-1, 1, 1}}}},
    });
    auto& upper = *mesh.GetLayer("Upper");
    ASSERT_TRUE(upper.SetLocalPose(Mesh->GetData().ReferencePose));
    ASSERT_TRUE(upper.Play(MakeNode(mirror), 0.5f));
    EXPECT_FALSE(mesh.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.1f);
    EXPECT_FLOAT_EQ(upper.GetTime(), 0);
    EXPECT_FLOAT_EQ(mesh.GetRootMotionDelta().Translation.x, 0.1f);
    ASSERT_TRUE(mesh.Update(0.1f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.2f);
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 0.1f, 1e-5f);
}

TEST_F(RootMotionTest, NamedRootUnderARotatedScaledWrapperMovesInMeshSpace)
{
    auto data = Mesh->GetData();
    data.ReferencePose.LocalTransforms[0].Translation = {5, 0, 0};
    data.ReferencePose.LocalTransforms[0].Rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    data.ReferencePose.LocalTransforms[0].Scale = glm::vec3(2);
    const auto wrapped = SkeletalMesh::Create(data);
    ASSERT_TRUE(wrapped);
    const auto clip = AnimationSequence::Create("ChildMotion", wrapped->GetSkeleton(), 1, {
        {.BoneIndex = 1, .Translations = {{0, {0, 0, 0}}, {1, {1, 0, 0}}}},
    });
    const auto meshHandle = AnimationTestCharacter::Create(wrapped);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true, "Child"));
    ASSERT_TRUE(mesh.Play(MakeNode(clip)));
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.x, 0, 1e-5f);
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.z, -0.5f, 1e-5f);
    EXPECT_EQ(mesh.GetLocalPose().LocalTransforms[1].Translation, data.ReferencePose.LocalTransforms[1].Translation);
    EXPECT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation, data.ReferencePose.LocalTransforms[0].Translation);
    EXPECT_FALSE(mesh.SetRootMotionEnabled(true, "Missing"));
    EXPECT_TRUE(mesh.IsRootMotionEnabled());
    ASSERT_TRUE(mesh.Update(0.25f));
    EXPECT_NEAR(mesh.GetRootMotionDelta().Translation.z, -0.5f, 1e-5f);
}

TEST_F(RootMotionTest, AnimatedRootAncestorsRejectTheUpdateWithoutAdvancingClocks)
{
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true, "Child"));
    ASSERT_TRUE(mesh.GetBaseLayer().Play(MakeNode(MakeClip(2))));
    EXPECT_FALSE(mesh.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0);
    ExpectNear(mesh.GetRootMotionDelta(), {});
}

TEST_F(RootMotionTest, NonuniformRootAncestorsRejectExtractionConfiguration)
{
    auto data = Mesh->GetData();
    data.ReferencePose.LocalTransforms[0].Scale = {1, 2, 1};
    const auto wrapped = SkeletalMesh::Create(data);
    ASSERT_TRUE(wrapped);
    const auto meshHandle = AnimationTestCharacter::Create(wrapped);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    EXPECT_FALSE(mesh.SetRootMotionEnabled(true, "Child"));
    EXPECT_FALSE(mesh.IsRootMotionEnabled());
    ASSERT_TRUE(mesh.SetRootMotionEnabled(true));
    EXPECT_TRUE(mesh.IsRootMotionEnabled());
}
