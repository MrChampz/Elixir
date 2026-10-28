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
        data.Skeleton = Skeleton::Create("References", {
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
        }, {{.Name = "TargetGrip", .BoneIndex = 7}, {.Name = "RightGrip", .BoneIndex = 6},
            {.Name = "LeftGrip", .BoneIndex = 3}});
        data.ReferencePose.LocalTransforms.clear();
        return SkeletalMesh::Create(std::move(data));
    }

    /** @brief Create a left-arm follower of the independent target bone. */
    STwoBoneIKConstraint Follower()
    {
        return {.EndBoneIndex = 3, .PolePosition = {-1, 2, 0},
            .TargetRotation = glm::quat(1, 0, 0, 0), .TargetReference = uint32_t(7)};
    }
}

TEST(IKTargetReferenceTest, BoneTargetsFollowTheCurrentAnimationFrameWithoutLag)
{
    const auto mesh = MakeReferenceMesh();
    const auto instanceHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    const auto sequence = Sequence::Create(
        "MoveTarget",
        mesh->GetSkeleton(),
        1,
        {{
            .BoneIndex = 7, .Translations = {{0, {0, 1, 0}}, {1, {0.3f, 0.5f, 0.2f}}},
            .Rotations = {{0, {1, 0, 0, 0}}, {1, glm::angleAxis(0.8f, glm::vec3(0, 0, 1))}}
        }}
    );
    ASSERT_TRUE(instance.Play(SequenceNode::Create(sequence), false));
    const auto follower = Follower();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    for (int frame = 0; frame < 4; ++frame)
    {
        ASSERT_TRUE(instance.Update(0.25f));
        const auto& model = instance.GetModelPose().Matrices;
        ExpectPosition(glm::vec3(model[3][3]), glm::vec3(model[7][3]));
        ExpectRotation(model[3], glm::quat_cast(glm::mat3(model[7])));
    }
}

TEST(IKTargetReferenceTest, SocketAndLocalOffsetsComposePositionRotationAndScale)
{
    auto data = MakeReferenceMesh()->GetData();
    data.ReferencePose.LocalTransforms[7].Rotation = glm::angleAxis(0.4f, glm::vec3(0, 0, 1));
    auto sockets = data.Skeleton->GetSockets();
    sockets[0].RelativeTransform = {.Translation = {0.1f, 0, 0},
        .Rotation = glm::angleAxis(0.5f, glm::vec3(0, 0, 1)), .Scale = {2, 2, 2}};
    data.Skeleton = data.Skeleton->WithSockets(std::move(sockets));
    const auto instanceHandle = MakeIKAnimation(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    follower.TargetPosition = {0.1f, 0, 0};
    follower.TargetRotation = glm::angleAxis(0.3f, glm::vec3(0, 1, 0));
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto* grip = instance.GetSkeleton()->GetSocket("TargetGrip");
    ASSERT_TRUE(grip);
    const auto socket = grip->GetTransform(instance.GetModelPose());
    ASSERT_TRUE(socket);
    ExpectPosition(
        glm::vec3(instance.GetModelPose().Matrices[3][3]),
        glm::vec3(*socket * glm::vec4(follower.TargetPosition, 1))
    );
    const auto rotation = glm::angleAxis(0.9f, glm::vec3(0, 0, 1)) * *follower.TargetRotation;
    ExpectRotation(instance.GetModelPose().Matrices[3], rotation);
}

TEST(IKTargetReferenceTest, PositionOnlyReferencesSupportMirroredNonuniformScale)
{
    auto data = MakeReferenceMesh()->GetData();
    data.ReferencePose.LocalTransforms[7].Scale = {-1, 2, 1};
    const auto instanceHandle = MakeIKAnimation(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    follower.TargetPosition = {0.2f, 0, 0};
    follower.TargetRotation.reset();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), {-0.2f, 1, 0});
}

TEST(IKTargetReferenceTest, RotationReferenceRejectsAmbiguousScaleWithoutAdvancingPlayback)
{
    const auto mesh = MakeReferenceMesh();
    const auto sequence = Sequence::Create(
        "ScaleTarget",
        mesh->GetSkeleton(),
        1,
        {{
            .BoneIndex = 7, .Scales = {{0, {1, 1, 1}}, {1, {2, 1, 1}}}
        }}
    );
    const auto instanceHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    ASSERT_TRUE(instance.Play(SequenceNode::Create(sequence)));
    const auto follower = Follower();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
}

TEST(IKTargetReferenceTest, SocketRotationRejectsNonuniformSocketScale)
{
    auto data = MakeReferenceMesh()->GetData();
    auto sockets = data.Skeleton->GetSockets();
    sockets[0].RelativeTransform.Scale = {1, 2, 1};
    data.Skeleton = data.Skeleton->WithSockets(std::move(sockets));
    const auto instanceHandle = MakeIKAnimation(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    EXPECT_FALSE(instance.Update(0));
    follower.TargetRotation.reset();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    EXPECT_TRUE(instance.Update(0));
}

TEST(IKTargetReferenceTest, DriverRunsBeforeItsFollowerInTheSameFrame)
{
    const auto instanceHandle = MakeIKAnimation(MakeReferenceMesh());
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    const STwoBoneIKConstraint driver{6, {0, 1, 0}, {1, 1, 0}, 1, glm::quat(1, 0, 0, 0)};
    auto follower = Follower();
    follower.TargetReference = std::string("RightGrip");
    const std::array constraints{driver, follower};
    ASSERT_TRUE(instanceIK->SetConstraints(constraints));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[6][3]), {0, 1, 0});
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), {0, 1, 0});
}

TEST(IKTargetReferenceTest, RejectsBackwardDependenciesFeedbackAndCycles)
{
    const auto instanceHandle = MakeIKAnimation(MakeReferenceMesh());
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    for (const auto bone : {1u, 2u, 3u})
    {
        auto self = follower;
        self.TargetReference = bone;
        EXPECT_FALSE(instanceIK->SetConstraints(std::span(&self, 1)));
    }
    follower.TargetReference = std::string("LeftGrip");
    EXPECT_FALSE(instanceIK->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = uint32_t(6);
    auto driver = STwoBoneIKConstraint{6, {0, 1, 0}, {1, 1, 0}};
    EXPECT_FALSE(instanceIK->SetConstraints(std::array{follower, driver}));
    driver.TargetReference = uint32_t(3);
    EXPECT_FALSE(instanceIK->SetConstraints(std::array{driver, follower}));
    ASSERT_EQ(instanceIK->GetConstraints().size(), 1);
    EXPECT_EQ(std::get<uint32_t>(*instanceIK->GetConstraints()[0].TargetReference), 7);
}

TEST(IKTargetReferenceTest, MutedDriversDoNotCreateAFalseDependency)
{
    const auto instanceHandle = MakeIKAnimation(MakeReferenceMesh());
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    follower.TargetReference = uint32_t(6);
    follower.TargetPosition = {-1, 0, 0};
    const STwoBoneIKConstraint muted{6, {0, 1, 0}, {1, 1, 0}, 0};
    ASSERT_TRUE(instanceIK->SetConstraints(std::array{follower, muted}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), {1, 0, 0});
}

TEST(IKTargetReferenceTest, RejectsUnknownReferencesIncludingMutedConstraints)
{
    const auto instanceHandle = MakeIKAnimation(MakeReferenceMesh());
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = uint32_t(100);
    EXPECT_FALSE(instanceIK->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = std::string("Unknown");
    follower.Weight = 0;
    EXPECT_FALSE(instanceIK->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference = std::string();
    EXPECT_FALSE(instanceIK->SetConstraints(std::span(&follower, 1)));
    EXPECT_EQ(std::get<uint32_t>(*instanceIK->GetConstraints()[0].TargetReference), 7);
}

TEST(IKTargetReferenceTest, PoleStaysInSkeletonSpaceAndReferencesCanBeRemoved)
{
    const auto instanceHandle = MakeIKAnimation(MakeReferenceMesh());
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    follower.TargetRotation.reset();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_GT(instance.GetModelPose().Matrices[2][3].y, 0);
    follower.TargetReference.reset();
    follower.TargetPosition = {0, -1, 0};
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), {0, -1, 0});
}

TEST(IKTargetReferenceTest, RootMotionLockIsAppliedBeforeResolvingReferences)
{
    const auto mesh = MakeReferenceMesh();
    const auto sequence = Sequence::Create(
        "MoveScene",
        mesh->GetSkeleton(),
        1,
        {{
            .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}
        }}
    );
    const auto instanceHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.Play(SequenceNode::Create(sequence), false));
    const auto follower = Follower();
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), {0, 1, 0});
    EXPECT_FLOAT_EQ(instance.GetRootMotionDelta().Translation.x, 2);
}

TEST(IKTargetReferenceTest, SeparateInstancesResolveTheirOwnAnimatedReferences)
{
    const auto mesh = MakeReferenceMesh();
    const auto firstHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(firstHandle);
    auto& first = *firstHandle;
    const auto firstIK = AttachIK(first);
    ASSERT_TRUE(firstIK);
    const auto secondHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    const auto secondIK = AttachIK(second);
    ASSERT_TRUE(secondIK);
    const auto follower = Follower();
    ASSERT_TRUE(firstIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(secondIK->SetConstraints(std::span(&follower, 1)));
    auto pose = first.GetLocalPose();
    pose.LocalTransforms[7].Translation = {0.2f, 0.8f, 0};
    ASSERT_TRUE(first.SetLocalPose(pose));
    ASSERT_TRUE(second.Update(0));
    ExpectPosition(glm::vec3(first.GetModelPose().Matrices[3][3]), {0.2f, 0.8f, 0});
    ExpectPosition(glm::vec3(second.GetModelPose().Matrices[3][3]), {0, 1, 0});
}

TEST(IKTargetReferenceTest, ReferencesIncludeRotatedUniformlyScaledSharedAncestors)
{
    auto data = MakeReferenceMesh()->GetData();
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(0, 1, 0));
    data.ReferencePose.LocalTransforms[0] = {.Translation = {0.2f, 0, 0.1f},
        .Rotation = rotation, .Scale = {2, 2, 2}};
    const auto instanceHandle = MakeIKAnimation(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto instanceIK = AttachIK(instance);
    ASSERT_TRUE(instanceIK);
    auto follower = Follower();
    follower.TargetPosition = {0.1f, 0, 0};
    follower.TargetRotation = glm::angleAxis(0.3f, glm::vec3(1, 0, 0));
    ASSERT_TRUE(instanceIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(
        glm::vec3(instance.GetModelPose().Matrices[3][3]),
        glm::vec3(instance.GetModelPose().Matrices[7] * glm::vec4(follower.TargetPosition, 1))
    );
    ExpectRotation(instance.GetModelPose().Matrices[3], rotation * *follower.TargetRotation);
}

TEST(IKTargetReferenceTest, RelativeTargetsUseTheSamePartialWeightAsSkeletonTargets)
{
    const auto mesh = MakeReferenceMesh();
    const auto relativeHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(relativeHandle);
    auto& relative = *relativeHandle;
    const auto relativeIK = AttachIK(relative);
    ASSERT_TRUE(relativeIK);
    const auto absoluteHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(absoluteHandle);
    auto& absolute = *absoluteHandle;
    const auto absoluteIK = AttachIK(absolute);
    ASSERT_TRUE(absoluteIK);
    auto follower = Follower();
    follower.Weight = 0.5f;
    ASSERT_TRUE(relativeIK->SetConstraints(std::span(&follower, 1)));
    follower.TargetReference.reset();
    follower.TargetPosition = {0, 1, 0};
    ASSERT_TRUE(absoluteIK->SetConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(relative.Update(0));
    ASSERT_TRUE(absolute.Update(0));
    ExpectPosition(
        glm::vec3(relative.GetModelPose().Matrices[3][3]),
        glm::vec3(absolute.GetModelPose().Matrices[3][3])
    );
    ExpectRotation(
        relative.GetModelPose().Matrices[3],
        glm::quat_cast(glm::mat3(absolute.GetModelPose().Matrices[3]))
    );
}

TEST(IKTargetReferenceTest, CompatibleRigsKeepTheSolversValidatedSocketDefinitions)
{
    const auto mesh = MakeReferenceMesh();
    ASSERT_TRUE(mesh);
    const auto& skeleton = mesh->GetSkeleton();
    const auto solver = TwoBoneIK::Create(skeleton);
    ASSERT_TRUE(solver);
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    ASSERT_TRUE(solver->SetConstraints(std::span(&follower, 1)));

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
        auto pose = mesh->GetData().ReferencePose;
        pose.LocalTransforms[7].Translation = {0.2f, 0.8f, 0};
        pose.LocalTransforms[7].Rotation = rotation;
        const auto applied = solver->Apply({*rig}, pose);
        EXPECT_TRUE(applied);
        if (!applied) continue;

        SModelPose model;
        ASSERT_TRUE(rig->BuildModelPose(pose, model));
        ExpectPosition(glm::vec3(model.Matrices[3][3]), {0.2f, 0.8f, 0});
        ExpectRotation(model.Matrices[3], rotation);
    }
}
