#include "TwoBoneIKTestUtils.h"
#include <Engine/Animation/SequenceNode.h>
#include "../Mesh/SkinningTestData.h"

using namespace Elixir;
using namespace Elixir::Tests::IK;

namespace
{
    /** @brief Create independent left and right limbs and an animated target branch. */
    Ref<SkeletalMesh> MakeReferenceMesh()
    {
        auto data = SkinningTests::MakeMesh()->GetData();
        data.Skeleton = Skeleton::Create(
            "References",
            {
                {.Name = "Scene"},
                {.Name = "Left", .ParentIndex = 0,
                    .ReferenceTransform = {.Translation = {-1, 0, 0}}},
                {.Name = "LeftMiddle", .ParentIndex = 1,
                    .ReferenceTransform = {.Translation = {1, 0, 0}}},
                {.Name = "LeftEnd", .ParentIndex = 2,
                    .ReferenceTransform = {.Translation = {1, 0, 0}}},
                {.Name = "Right", .ParentIndex = 0},
                {.Name = "RightMiddle", .ParentIndex = 4,
                    .ReferenceTransform = {.Translation = {1, 0, 0}}},
                {.Name = "RightEnd", .ParentIndex = 5,
                    .ReferenceTransform = {.Translation = {1, 0, 0}}},
                {.Name = "Target", .ParentIndex = 0,
                    .ReferenceTransform = {.Translation = {0, 1, 0}}}
            },
            {
                {.Name = "TargetGrip", .BoneIndex = 7},
                {.Name = "RightGrip", .BoneIndex = 6},
                {.Name = "LeftGrip", .BoneIndex = 3}
            }
        );
        data.ReferencePose.LocalTransforms.clear();
        return SkeletalMesh::Create(std::move(data));
    }

    /**
     * @brief Replace the target socket offset in mesh data used by a test.
     * @param data Reference mesh data whose first socket is TargetGrip.
     * @param transform Position, rotation, and scale relative to the target bone.
     * @return False if the socket definition is invalid; retain the previous skeleton.
     */
    bool ConfigureTargetSocket(SSkeletalMeshData& data, const SBoneTransform& transform)
    {
        auto sockets = data.Skeleton->GetSockets();
        sockets[0].RelativeTransform = transform;
        auto skeleton = data.Skeleton->WithSockets(std::move(sockets));
        if (!skeleton) return false;
        data.Skeleton = std::move(skeleton);
        return true;
    }

    /** Supplies independent limbs, an animated target, and an attached IK modifier. */
    class IKTargetReferenceTest : public TwoBoneIKFixture
    {
    protected:
        /** @brief Create the reference rig before each test. */
        void SetUp() override
        {
            ASSERT_TRUE(Initialize(MakeReferenceMesh()));
        }
    };

    /** @brief Create a left-arm follower of the independent target bone. */
    STwoBoneIKConstraint Follower()
    {
        return {.EndBoneIndex = 3, .PolePosition = {-1, 2, 0},
            .TargetRotation = glm::quat(1, 0, 0, 0), .TargetReference = uint32_t(7)};
    }
}

TEST_F(IKTargetReferenceTest, BoneTargetsFollowTheCurrentAnimationFrameWithoutLag)
{
    const auto sequence = Sequence::Create(
        "MoveTarget",
        Mesh->GetSkeleton(),
        1,
        {{
            .BoneIndex = 7, .Translations = {{0, {0, 1, 0}}, {1, {0.3f, 0.5f, 0.2f}}},
            .Rotations = {{0, {1, 0, 0, 0}}, {1, glm::angleAxis(0.8f, glm::vec3(0, 0, 1))}}
        }}
    );
    ASSERT_TRUE(Playback->Play(SequenceNode::Create(sequence), false));
    const auto follower = Follower();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    for (int frame = 0; frame < 4; ++frame)
    {
        ASSERT_TRUE(Playback->Update(0.25f));
        const auto& model = Playback->GetModelPose().Matrices;
        ExpectPosition(glm::vec3(model[3][3]), glm::vec3(model[7][3]));
        ExpectRotation(model[3], glm::quat_cast(glm::mat3(model[7])));
    }
}

TEST_F(IKTargetReferenceTest, SocketAndLocalOffsetsComposePositionRotationAndScale)
{
    auto data = Mesh->GetData();
    data.ReferencePose.LocalTransforms[7].Rotation = glm::angleAxis(0.4f, glm::vec3(0, 0, 1));
    ASSERT_TRUE(ConfigureTargetSocket(
        data,
        {
            .Translation = {0.1f, 0, 0},
            .Rotation = glm::angleAxis(0.5f, glm::vec3(0, 0, 1)),
            .Scale = {2, 2, 2}
        }
    ));
    ASSERT_TRUE(Initialize(SkeletalMesh::Create(std::move(data))));
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    follower.TargetPosition = {0.1f, 0, 0};
    follower.TargetRotation = glm::angleAxis(0.3f, glm::vec3(0, 1, 0));
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    const auto* grip = Playback->GetSkeleton()->GetSocket("TargetGrip");
    ASSERT_TRUE(grip);
    const auto socket = grip->GetTransform(Playback->GetModelPose());
    ASSERT_TRUE(socket);
    ExpectPosition(
        glm::vec3(Playback->GetModelPose().Matrices[3][3]),
        glm::vec3(*socket * glm::vec4(follower.TargetPosition, 1))
    );
    const auto rotation = glm::angleAxis(0.9f, glm::vec3(0, 0, 1)) * *follower.TargetRotation;
    ExpectRotation(Playback->GetModelPose().Matrices[3], rotation);
}

TEST_F(IKTargetReferenceTest, PositionOnlyReferencesSupportMirroredNonuniformScale)
{
    auto data = Mesh->GetData();
    data.ReferencePose.LocalTransforms[7].Scale = {-1, 2, 1};
    ASSERT_TRUE(Initialize(SkeletalMesh::Create(std::move(data))));
    auto follower = Follower();
    follower.TargetPosition = {0.2f, 0, 0};
    follower.TargetRotation.reset();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), {-0.2f, 1, 0});
}

TEST_F(IKTargetReferenceTest, RotationReferenceRejectsAmbiguousScaleWithoutAdvancingPlayback)
{
    const auto sequence = Sequence::Create(
        "ScaleTarget",
        Mesh->GetSkeleton(),
        1,
        {{
            .BoneIndex = 7, .Scales = {{0, {1, 1, 1}}, {1, {2, 1, 1}}}
        }}
    );
    ASSERT_TRUE(Playback->Play(SequenceNode::Create(sequence)));
    const auto follower = Follower();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    const auto pose = Playback->GetModelPose().Matrices;
    EXPECT_FALSE(Playback->Update(0.5f));
    EXPECT_EQ(Playback->GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0);
}

TEST_F(IKTargetReferenceTest, SocketRotationRejectsNonuniformSocketScale)
{
    auto data = Mesh->GetData();
    ASSERT_TRUE(ConfigureTargetSocket(data, {.Scale = {1, 2, 1}}));
    ASSERT_TRUE(Initialize(SkeletalMesh::Create(std::move(data))));
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    EXPECT_FALSE(Playback->Update(0));
    follower.TargetRotation.reset();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    EXPECT_TRUE(Playback->Update(0));
}

TEST_F(IKTargetReferenceTest, DriverRunsBeforeItsFollowerInTheSameFrame)
{
    const STwoBoneIKConstraint driver{6, {0, 1, 0}, {1, 1, 0}, 1, glm::quat(1, 0, 0, 0)};
    auto follower = Follower();
    follower.TargetReference = std::string("RightGrip");
    const std::array constraints{driver, follower};
    ASSERT_TRUE(Solver->SetConstraints(constraints));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[6][3]), {0, 1, 0});
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), {0, 1, 0});
}

TEST_F(IKTargetReferenceTest, RejectsBackwardDependenciesFeedbackAndCycles)
{
    auto follower = Follower();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    for (const auto bone : {1u, 2u, 3u})
    {
        auto self = follower;
        self.TargetReference = bone;
        EXPECT_FALSE(Solver->SetConstraints(std::span(&self, 1)));
    }
    follower.TargetReference = std::string("LeftGrip");
    EXPECT_FALSE(Solver->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = uint32_t(6);
    auto driver = STwoBoneIKConstraint{6, {0, 1, 0}, {1, 1, 0}};
    EXPECT_FALSE(Solver->SetConstraints(std::array{follower, driver}));
    driver.TargetReference = uint32_t(3);
    EXPECT_FALSE(Solver->SetConstraints(std::array{driver, follower}));
    ASSERT_EQ(Solver->GetConstraints().size(), 1);
    EXPECT_EQ(std::get<uint32_t>(*Solver->GetConstraints()[0].TargetReference), 7);
}

TEST_F(IKTargetReferenceTest, MutedDriversDoNotCreateAFalseDependency)
{
    auto follower = Follower();
    follower.TargetReference = uint32_t(6);
    follower.TargetPosition = {-1, 0, 0};
    const STwoBoneIKConstraint muted{6, {0, 1, 0}, {1, 1, 0}, 0};
    ASSERT_TRUE(Solver->SetConstraints(std::array{follower, muted}));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), {1, 0, 0});
}

TEST_F(IKTargetReferenceTest, RejectsUnknownReferencesIncludingMutedConstraints)
{
    auto follower = Follower();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = uint32_t(100);
    EXPECT_FALSE(Solver->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = std::string("Unknown");
    follower.Weight = 0;
    EXPECT_FALSE(Solver->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = std::string();
    EXPECT_FALSE(Solver->SetConstraints(std::span(&follower, 1)));
    EXPECT_EQ(std::get<uint32_t>(*Solver->GetConstraints()[0].TargetReference), 7);
}

TEST_F(IKTargetReferenceTest, PoleStaysInSkeletonSpaceAndReferencesCanBeRemoved)
{
    auto follower = Follower();
    follower.TargetRotation.reset();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    EXPECT_GT(Playback->GetModelPose().Matrices[2][3].y, 0);
    follower.TargetReference.reset();
    follower.TargetPosition = {0, -1, 0};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), {0, -1, 0});
}

TEST_F(IKTargetReferenceTest, RootMotionLockIsAppliedBeforeResolvingReferences)
{
    const auto sequence = Sequence::Create(
        "MoveScene",
        Mesh->GetSkeleton(),
        1,
        {{
            .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}
        }}
    );
    ASSERT_TRUE(Playback->SetRootMotionEnabled(true));
    ASSERT_TRUE(Playback->Play(SequenceNode::Create(sequence), false));
    const auto follower = Follower();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0.5f));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), {0, 1, 0});
    EXPECT_FLOAT_EQ(Playback->GetRootMotionDelta().Translation.x, 2);
}

TEST_F(IKTargetReferenceTest, SeparateInstancesResolveTheirOwnAnimatedReferences)
{
    const auto secondHandle = MakeIKAnimation(Mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    const auto secondIK = AttachIK(second);
    ASSERT_TRUE(secondIK);
    const auto follower = Follower();
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(secondIK->SetConstraints(std::span(&follower, 1)));
    auto pose = Playback->GetLocalPose();
    pose.LocalTransforms[7].Translation = {0.2f, 0.8f, 0};
    ASSERT_TRUE(Playback->SetLocalPose(pose));
    ASSERT_TRUE(second.Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), {0.2f, 0.8f, 0});
    ExpectPosition(glm::vec3(second.GetModelPose().Matrices[3][3]), {0, 1, 0});
}

TEST_F(IKTargetReferenceTest, ReferencesIncludeRotatedUniformlyScaledSharedAncestors)
{
    auto data = Mesh->GetData();
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(0, 1, 0));
    data.ReferencePose.LocalTransforms[0] = {.Translation = {0.2f, 0, 0.1f},
        .Rotation = rotation, .Scale = {2, 2, 2}};
    ASSERT_TRUE(Initialize(SkeletalMesh::Create(std::move(data))));
    auto follower = Follower();
    follower.TargetPosition = {0.1f, 0, 0};
    follower.TargetRotation = glm::angleAxis(0.3f, glm::vec3(1, 0, 0));
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(
        glm::vec3(Playback->GetModelPose().Matrices[3][3]),
        glm::vec3(Playback->GetModelPose().Matrices[7] * glm::vec4(follower.TargetPosition, 1))
    );
    ExpectRotation(Playback->GetModelPose().Matrices[3], rotation * *follower.TargetRotation);
}

TEST_F(IKTargetReferenceTest, RelativeTargetsUseTheSamePartialWeightAsSkeletonTargets)
{
    const auto absoluteHandle = MakeIKAnimation(Mesh);
    ASSERT_TRUE(absoluteHandle);
    auto& absolute = *absoluteHandle;
    const auto absoluteIK = AttachIK(absolute);
    ASSERT_TRUE(absoluteIK);
    auto follower = Follower();
    follower.Weight = 0.5f;
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference.reset();
    follower.TargetPosition = {0, 1, 0};
    ASSERT_TRUE(absoluteIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ASSERT_TRUE(absolute.Update(0));
    ExpectPosition(
        glm::vec3(Playback->GetModelPose().Matrices[3][3]),
        glm::vec3(absolute.GetModelPose().Matrices[3][3])
    );
    ExpectRotation(
        Playback->GetModelPose().Matrices[3],
        glm::quat_cast(glm::mat3(absolute.GetModelPose().Matrices[3]))
    );
}

TEST_F(IKTargetReferenceTest, CompatibleRigsKeepTheSolversValidatedSocketDefinitions)
{
    const auto& skeleton = Mesh->GetSkeleton();
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    ASSERT_TRUE(Solver->SetConstraints(std::span(&follower, 1)));

    const std::array rigs{
        skeleton->WithSockets({}),
        skeleton->WithSockets({{.Name = "TargetGrip", .BoneIndex = 3}})
    };
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(0, 0, 1));
    for (const auto& rig : rigs)
    {
        ASSERT_TRUE(rig);
        SCOPED_TRACE(rig->GetSockets().empty() ? "Absent socket" : "Socket in follower chain");
        ASSERT_TRUE(skeleton->IsCompatible(*rig));
        auto pose = Mesh->GetData().ReferencePose;
        pose.LocalTransforms[7].Translation = {0.2f, 0.8f, 0};
        pose.LocalTransforms[7].Rotation = rotation;
        const auto applied = Solver->Apply({*rig}, pose);
        EXPECT_TRUE(applied);
        if (!applied) continue;

        SModelPose model;
        ASSERT_TRUE(rig->BuildModelPose(pose, model));
        ExpectPosition(glm::vec3(model.Matrices[3][3]), {0.2f, 0.8f, 0});
        ExpectRotation(model.Matrices[3], rotation);
    }
}
