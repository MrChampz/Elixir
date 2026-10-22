#include <gtest/gtest.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    /** @brief Create a mesh with a socket on its second bone. */
    Ref<SkeletalMesh> MakeSocketMesh()
    {
        auto data = SkinningTests::MakeMesh()->GetData();
        data.Skeleton = data.Skeleton->WithSockets({{
            .Name = "Grip", .BoneIndex = 1,
            .RelativeTransform = { .Translation = {1, 0, 0} }
        }});
        return SkeletalMesh::Create(std::move(data));
    }

    /**
     * @brief Compare all matrix components, including rotation and scale.
     * @param actual Observed transform.
     * @param expected Expected transform.
     */
    void ExpectMatrix(const glm::mat4& actual, const glm::mat4& expected)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                EXPECT_NEAR(actual[c][r], expected[c][r], 1e-5f);
    }
}

TEST(SkeletonSocketTest, ValidatesNamesBonesAndTransforms)
{
    const auto skeleton = SkinningTests::MakeMesh()->GetSkeleton();
    EXPECT_FALSE(skeleton->WithSockets({{.Name = ""}}));
    EXPECT_FALSE(skeleton->WithSockets({{.Name = "Grip"}, {.Name = "Grip"}}));
    EXPECT_FALSE(skeleton->WithSockets({{.Name = "Grip", .BoneIndex = 2}}));
    auto socket = SSkeletonSocket{.Name = "Grip"};
    socket.RelativeTransform.Translation.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(skeleton->WithSockets({socket}));
    socket.RelativeTransform = {};
    socket.RelativeTransform.Rotation = {0, 0, 0, 0};
    EXPECT_FALSE(skeleton->WithSockets({socket}));
    socket.RelativeTransform = {};
    socket.RelativeTransform.Scale.y = 0;
    EXPECT_FALSE(skeleton->WithSockets({socket}));
    socket.RelativeTransform.Scale.y = -1;
    EXPECT_TRUE(skeleton->WithSockets({socket}));
}

TEST(SkeletonSocketTest, ClonePreservesHierarchyAndAnimationCompatibility)
{
    const auto skeleton = SkinningTests::MakeMesh()->GetSkeleton();
    const auto clone = skeleton->WithSockets({{.Name = "Root"}, {.Name = "Grip", .BoneIndex = 1}});
    ASSERT_TRUE(clone);
    EXPECT_TRUE(skeleton->GetSockets().empty());
    EXPECT_TRUE(skeleton->IsCompatible(*clone));
    EXPECT_EQ(clone->GetName(), skeleton->GetName());
    EXPECT_EQ(clone->FindSocket("Root"), 0);
    EXPECT_EQ(clone->FindSocket("Grip"), 1);
    EXPECT_EQ(clone->FindSocket("grip"), -1);
    EXPECT_TRUE(clone->WithSockets({})->GetSockets().empty());
    EXPECT_EQ(clone->GetSockets().size(), 2);
}

TEST(SkeletonSocketTest, CreateAcceptsSocketsWithoutCloning)
{
    const auto skeleton = Skeleton::Create("Rig", {{.Name = "Root"}}, {{.Name = "Grip"}});
    ASSERT_TRUE(skeleton);
    EXPECT_EQ(skeleton->FindSocket("Grip"), 0);
}

TEST(SkeletonSocketTest, MissingMeshOrSocketReturnsEmpty)
{
    const SkeletalMeshInstance empty(nullptr);
    EXPECT_FALSE(empty.GetSocketTransform("Grip"));
    EXPECT_FALSE(empty.GetSocketTransform(size_t(0)));
    const SkeletalMeshInstance instance(MakeSocketMesh());
    EXPECT_FALSE(instance.GetSocketTransform("Unknown"));
    EXPECT_FALSE(instance.GetSocketTransform(size_t(-1)));
    EXPECT_FALSE(instance.GetSocketTransform(size_t(1)));
}

TEST(SkeletonSocketTest, UsesHierarchyAndLocalOffsetWithoutInverseBindMatrices)
{
    auto data = MakeSocketMesh()->GetData();
    data.ReferencePose[0] = {.Translation = {2, 3, 4},
        .Rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1)),
        .Scale = {2, 3, 4}};
    data.ReferencePose[1].Translation = {0, 1, 0};
    auto socket = data.Skeleton->GetSockets()[0];
    socket.RelativeTransform.Rotation = glm::angleAxis(0.4f, glm::vec3(1, 0, 0));
    socket.RelativeTransform.Scale = {1, 2, -1};
    data.Skeleton = data.Skeleton->WithSockets({socket});
    const auto expected = data.ReferencePose[0].ToMatrix() * data.ReferencePose[1].ToMatrix() *
        socket.RelativeTransform.ToMatrix();
    const SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    const auto transform = instance.GetSocketTransform("Grip");
    ASSERT_TRUE(transform);
    ExpectMatrix(*transform, expected);
    ExpectMatrix(*instance.GetSocketTransform(size_t(0)), expected);
    const auto world = glm::translate(glm::mat4(1), glm::vec3(10, 0, 0));
    ExpectMatrix(world * *transform, world * expected);
}

TEST(SkeletonSocketTest, FollowsAnimationAndIndependentInstancePoses)
{
    const auto mesh = MakeSocketMesh();
    const auto sequence = AnimationSequence::Create("Move", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 1, .Translations = {{0, {0, 0, 0}}, {1, {0, 4, 0}}}
    }});
    SkeletalMeshInstance first(mesh), second(mesh);
    ASSERT_TRUE(first.Play(sequence, false));
    ASSERT_TRUE(first.Update(0.5f));
    ExpectMatrix(*first.GetSocketTransform("Grip"),
        glm::translate(glm::mat4(1), glm::vec3(1, 2, 0)));
    ExpectMatrix(*second.GetSocketTransform("Grip"),
        glm::translate(glm::mat4(1), glm::vec3(1, 0, 0)));
    first.Pause();
    ASSERT_TRUE(first.Update(0.25f));
    EXPECT_FLOAT_EQ((*first.GetSocketTransform("Grip"))[3].y, 2);
    ASSERT_TRUE(first.Seek(0.75f));
    EXPECT_FLOAT_EQ((*first.GetSocketTransform("Grip"))[3].y, 3);
    first.Stop();
    EXPECT_FLOAT_EQ((*first.GetSocketTransform("Grip"))[3].y, 0);
}

TEST(SkeletonSocketTest, ReadsFinalMaskedAdditiveAndMontagePose)
{
    const auto mesh = MakeSocketMesh();
    SkeletalMeshInstance instance(mesh);
    const auto mask = BoneMask::FromBone("Hand", mesh->GetSkeleton(), "Child");
    ASSERT_TRUE(instance.AddLayer("Action", mask, 0.5f));
    auto* layer = instance.GetLayer("Action");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    const auto sequence = AnimationSequence::Create("Action", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 1, .Translations = {{0, {0, 4, 0}}, {1, {0, 4, 0}}}
    }});
    const auto montage = AnimationMontage::Create({.Name = "Action", .Sequence = sequence,
        .BlendIn = 0, .BlendOut = 0});
    ASSERT_TRUE(layer->PlayMontage(montage));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ((*instance.GetSocketTransform("Grip"))[3].y, 2);
    ExpectMatrix(*instance.GetSocketTransform("Grip"), instance.GetModelPose()[1] *
        mesh->GetSkeleton()->GetSockets()[0].RelativeTransform.ToMatrix());
}

TEST(SkeletonSocketTest, UsesRootLockedPoseAndLeavesWorldMovementToCaller)
{
    const auto mesh = MakeSocketMesh();
    SkeletalMeshInstance instance(mesh);
    const auto sequence = AnimationSequence::Create("Move", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}
    }});
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(instance.Play(sequence, false));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto socket = instance.GetSocketTransform("Grip");
    ASSERT_TRUE(socket);
    EXPECT_FLOAT_EQ((*socket)[3].x, 1);
    EXPECT_FLOAT_EQ((instance.GetRootMotionDelta().ToMatrix() * *socket)[3].x, 3);
}

TEST(SkeletonSocketTest, FailedPoseChangesKeepLastAcceptedSocket)
{
    const auto mesh = MakeSocketMesh();
    SkeletalMeshInstance instance(mesh);
    const auto before = instance.GetSocketTransform("Grip");
    auto pose = instance.GetLocalPose();
    pose[1].Scale.x = 0;
    EXPECT_FALSE(instance.SetLocalPose(pose));
    EXPECT_FALSE(instance.Update(-1));
    ExpectMatrix(*instance.GetSocketTransform("Grip"), *before);
}

TEST(SkeletonSocketTest, RejectsOverflowWhenCombiningFinitePoseAndOffset)
{
    auto data = MakeSocketMesh()->GetData();
    data.ReferencePose[0].Translation.x = std::numeric_limits<float>::max();
    auto socket = data.Skeleton->GetSockets()[0];
    socket.RelativeTransform.Translation.x = std::numeric_limits<float>::max();
    data.Skeleton = data.Skeleton->WithSockets({socket});
    const auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    const SkeletalMeshInstance instance(mesh);
    EXPECT_FALSE(instance.GetSocketTransform("Grip"));
    EXPECT_FALSE(instance.GetSocketTransform(size_t(0)));
}
