#include <gtest/gtest.h>
#include "../Mesh/SkinningTestData.h"

using namespace Elixir;

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
        data.ReferencePose.clear();
        return SkeletalMesh::Create(std::move(data));
    }

    /**
     * @brief Compare float positions after solving a limb.
     * @param actual Solved end position.
     * @param expected Current reference-space target transformed into skeleton space.
     */
    void ExpectPosition(const glm::vec3& actual, const glm::vec3& expected)
    {
        for (int axis = 0; axis < 3; ++axis) EXPECT_NEAR(actual[axis], expected[axis], 1e-4f);
    }

    /**
     * @brief Compare matrix rotation axes after removing uniform scale.
     * @param actual Solved end matrix.
     * @param expected Skeleton-space target quaternion.
     */
    void ExpectRotation(const glm::mat4& actual, const glm::quat& expected)
    {
        const auto rotation = glm::mat3_cast(expected);
        for (int axis = 0; axis < 3; ++axis)
            ExpectPosition(glm::normalize(glm::vec3(actual[axis])), rotation[axis]);
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
    SkeletalMeshInstance instance(mesh);
    const auto sequence = AnimationSequence::Create("MoveTarget", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 7, .Translations = {{0, {0, 1, 0}}, {1, {0.3f, 0.5f, 0.2f}}},
        .Rotations = {{0, {1, 0, 0, 0}}, {1, glm::angleAxis(0.8f, glm::vec3(0, 0, 1))}}
    }});
    ASSERT_TRUE(instance.Play(sequence, false));
    const auto follower = Follower();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    for (int frame = 0; frame < 4; ++frame)
    {
        ASSERT_TRUE(instance.Update(0.25f));
        const auto& model = instance.GetModelPose();
        ExpectPosition(glm::vec3(model[3][3]), glm::vec3(model[7][3]));
        ExpectRotation(model[3], glm::quat_cast(glm::mat3(model[7])));
    }
}

TEST(IKTargetReferenceTest, SocketAndLocalOffsetsComposePositionRotationAndScale)
{
    auto data = MakeReferenceMesh()->GetData();
    data.ReferencePose[7].Rotation = glm::angleAxis(0.4f, glm::vec3(0, 0, 1));
    auto sockets = data.Skeleton->GetSockets();
    sockets[0].RelativeTransform = {.Translation = {0.1f, 0, 0},
        .Rotation = glm::angleAxis(0.5f, glm::vec3(0, 0, 1)), .Scale = {2, 2, 2}};
    data.Skeleton = data.Skeleton->WithSockets(std::move(sockets));
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    follower.TargetPosition = {0.1f, 0, 0};
    follower.TargetRotation = glm::angleAxis(0.3f, glm::vec3(0, 1, 0));
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto socket = *instance.GetSocketTransform("TargetGrip");
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]),
        glm::vec3(socket * glm::vec4(follower.TargetPosition, 1)));
    const auto rotation = glm::angleAxis(0.9f, glm::vec3(0, 0, 1)) * *follower.TargetRotation;
    ExpectRotation(instance.GetModelPose()[3], rotation);
}

TEST(IKTargetReferenceTest, PositionOnlyReferencesSupportMirroredNonuniformScale)
{
    auto data = MakeReferenceMesh()->GetData();
    data.ReferencePose[7].Scale = {-1, 2, 1};
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    auto follower = Follower();
    follower.TargetPosition = {0.2f, 0, 0};
    follower.TargetRotation.reset();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), {-0.2f, 1, 0});
}

TEST(IKTargetReferenceTest, RotationReferenceRejectsAmbiguousScaleWithoutAdvancingPlayback)
{
    const auto mesh = MakeReferenceMesh();
    const auto sequence = AnimationSequence::Create("ScaleTarget", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 7, .Scales = {{0, {1, 1, 1}}, {1, {2, 1, 1}}}
    }});
    SkeletalMeshInstance instance(mesh);
    ASSERT_TRUE(instance.Play(sequence));
    const auto follower = Follower();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
}

TEST(IKTargetReferenceTest, SocketRotationRejectsNonuniformSocketScale)
{
    auto data = MakeReferenceMesh()->GetData();
    auto sockets = data.Skeleton->GetSockets();
    sockets[0].RelativeTransform.Scale = {1, 2, 1};
    data.Skeleton = data.Skeleton->WithSockets(std::move(sockets));
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    auto follower = Follower();
    follower.TargetReference = std::string("TargetGrip");
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    EXPECT_FALSE(instance.Update(0));
    follower.TargetRotation.reset();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    EXPECT_TRUE(instance.Update(0));
}

TEST(IKTargetReferenceTest, DriverRunsBeforeItsFollowerInTheSameFrame)
{
    SkeletalMeshInstance instance(MakeReferenceMesh());
    const STwoBoneIKConstraint driver{6, {0, 1, 0}, {1, 1, 0}, 1, glm::quat(1, 0, 0, 0)};
    auto follower = Follower();
    follower.TargetReference = std::string("RightGrip");
    const std::array constraints{driver, follower};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(constraints));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[6][3]), {0, 1, 0});
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), {0, 1, 0});
}

TEST(IKTargetReferenceTest, RejectsBackwardDependenciesFeedbackAndCycles)
{
    SkeletalMeshInstance instance(MakeReferenceMesh());
    auto follower = Follower();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    for (const auto bone : {1u, 2u, 3u})
    {
        auto self = follower;
        self.TargetReference = bone;
        EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&self, 1)));
    }
    follower.TargetReference = std::string("LeftGrip");
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    follower.TargetReference = uint32_t(6);
    auto driver = STwoBoneIKConstraint{6, {0, 1, 0}, {1, 1, 0}};
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::array{follower, driver}));
    driver.TargetReference = uint32_t(3);
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::array{driver, follower}));
    ASSERT_EQ(instance.GetTwoBoneIKConstraints().size(), 1);
    EXPECT_EQ(std::get<uint32_t>(*instance.GetTwoBoneIKConstraints()[0].TargetReference), 7);
}

TEST(IKTargetReferenceTest, MutedDriversDoNotCreateAFalseDependency)
{
    SkeletalMeshInstance instance(MakeReferenceMesh());
    auto follower = Follower();
    follower.TargetReference = uint32_t(6);
    follower.TargetPosition = {-1, 0, 0};
    const STwoBoneIKConstraint muted{6, {0, 1, 0}, {1, 1, 0}, 0};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::array{follower, muted}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), {1, 0, 0});
}

TEST(IKTargetReferenceTest, RejectsUnknownReferencesIncludingMutedConstraints)
{
    SkeletalMeshInstance instance(MakeReferenceMesh());
    auto follower = Follower();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    follower.TargetReference = uint32_t(100);
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    follower.TargetReference = std::string("Unknown");
    follower.Weight = 0;
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    follower.TargetReference = std::string();
    EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    EXPECT_EQ(std::get<uint32_t>(*instance.GetTwoBoneIKConstraints()[0].TargetReference), 7);
}

TEST(IKTargetReferenceTest, PoleStaysInSkeletonSpaceAndReferencesCanBeRemoved)
{
    SkeletalMeshInstance instance(MakeReferenceMesh());
    auto follower = Follower();
    follower.TargetRotation.reset();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_GT(instance.GetModelPose()[2][3].y, 0);
    follower.TargetReference.reset();
    follower.TargetPosition = {0, -1, 0};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), {0, -1, 0});
}

TEST(IKTargetReferenceTest, RootMotionLockIsAppliedBeforeResolvingReferences)
{
    const auto mesh = MakeReferenceMesh();
    const auto sequence = AnimationSequence::Create("MoveScene", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}
    }});
    SkeletalMeshInstance instance(mesh);
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.Play(sequence, false));
    const auto follower = Follower();
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), {0, 1, 0});
    EXPECT_FLOAT_EQ(instance.GetRootMotionDelta().Translation.x, 2);
}

TEST(IKTargetReferenceTest, SeparateInstancesResolveTheirOwnAnimatedReferences)
{
    const auto mesh = MakeReferenceMesh();
    SkeletalMeshInstance first(mesh), second(mesh);
    const auto follower = Follower();
    ASSERT_TRUE(first.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(second.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    auto pose = first.GetLocalPose();
    pose[7].Translation = {0.2f, 0.8f, 0};
    ASSERT_TRUE(first.SetLocalPose(pose));
    ASSERT_TRUE(second.Update(0));
    ExpectPosition(glm::vec3(first.GetModelPose()[3][3]), {0.2f, 0.8f, 0});
    ExpectPosition(glm::vec3(second.GetModelPose()[3][3]), {0, 1, 0});
}

TEST(IKTargetReferenceTest, ReferencesIncludeRotatedUniformlyScaledSharedAncestors)
{
    auto data = MakeReferenceMesh()->GetData();
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(0, 1, 0));
    data.ReferencePose[0] = {.Translation = {0.2f, 0, 0.1f},
        .Rotation = rotation, .Scale = {2, 2, 2}};
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    auto follower = Follower();
    follower.TargetPosition = {0.1f, 0, 0};
    follower.TargetRotation = glm::angleAxis(0.3f, glm::vec3(1, 0, 0));
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]),
        glm::vec3(instance.GetModelPose()[7] * glm::vec4(follower.TargetPosition, 1)));
    ExpectRotation(instance.GetModelPose()[3], rotation * *follower.TargetRotation);
}

TEST(IKTargetReferenceTest, RelativeTargetsUseTheSamePartialWeightAsSkeletonTargets)
{
    const auto mesh = MakeReferenceMesh();
    SkeletalMeshInstance relative(mesh), absolute(mesh);
    auto follower = Follower();
    follower.Weight = 0.5f;
    ASSERT_TRUE(relative.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    follower.TargetReference.reset();
    follower.TargetPosition = {0, 1, 0};
    ASSERT_TRUE(absolute.SetTwoBoneIKConstraints(std::span(&follower, 1)));
    ASSERT_TRUE(relative.Update(0));
    ASSERT_TRUE(absolute.Update(0));
    ExpectPosition(glm::vec3(relative.GetModelPose()[3][3]),
        glm::vec3(absolute.GetModelPose()[3][3]));
    ExpectRotation(relative.GetModelPose()[3],
        glm::quat_cast(glm::mat3(absolute.GetModelPose()[3])));
}
