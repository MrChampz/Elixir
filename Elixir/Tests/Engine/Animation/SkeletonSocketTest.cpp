#include <gtest/gtest.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/SequenceNode.h>

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
    EXPECT_EQ(clone->GetSocket("Root"), &clone->GetSockets()[0]);
    EXPECT_EQ(clone->GetSocket("Grip"), &clone->GetSockets()[1]);
    EXPECT_EQ(clone->GetSocket("grip"), nullptr);
    EXPECT_TRUE(clone->WithSockets({})->GetSockets().empty());
    EXPECT_EQ(clone->GetSockets().size(), 2);
}

TEST(SkeletonSocketTest, CreateAcceptsSocketsWithoutCloning)
{
    const auto skeleton = Skeleton::Create("Rig", {{.Name = "Root"}}, {{.Name = "Grip"}});
    ASSERT_TRUE(skeleton);
    EXPECT_EQ(skeleton->GetSocket("Grip"), &skeleton->GetSockets()[0]);
}

TEST(SkeletonSocketTest, MissingSocketOrInvalidPoseReturnsEmpty)
{
    const auto mesh = MakeSocketMesh();
    const auto instance = SkeletalMeshInstance::Create(mesh);
    ASSERT_TRUE(instance);
    const auto& skeleton = mesh->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto& pose = instance->GetReferenceModelPose();
    EXPECT_EQ(skeleton->GetSocket("Unknown"), nullptr);
    EXPECT_FALSE(grip->GetTransform({}));
    auto invalid = pose;
    invalid.Matrices[1][0].x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(grip->GetTransform(invalid));
    invalid = pose;
    invalid.Matrices.resize(1);
    EXPECT_FALSE(grip->GetTransform(invalid));
}

TEST(SkeletonSocketTest, UsesHierarchyAndLocalOffsetWithoutInverseBindMatrices)
{
    auto data = MakeSocketMesh()->GetData();
    auto& reference = data.ReferencePose.LocalTransforms;
    reference[0] = {
        .Translation = {2, 3, 4},
        .Rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1)),
        .Scale = {2, 3, 4}
    };
    reference[1].Translation = {0, 1, 0};
    auto socket = data.Skeleton->GetSockets()[0];
    socket.RelativeTransform.Rotation = glm::angleAxis(0.4f, glm::vec3(1, 0, 0));
    socket.RelativeTransform.Scale = {1, 2, -1};
    data.Skeleton = data.Skeleton->WithSockets({socket});
    const auto expected = reference[0].ToMatrix() * reference[1].ToMatrix() *
        socket.RelativeTransform.ToMatrix();
    const auto instance = SkeletalMeshInstance::Create(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(instance);
    const auto& skeleton = instance->GetMesh()->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto& pose = instance->GetReferenceModelPose();
    const auto transform = grip->GetTransform(pose);
    ASSERT_TRUE(transform);
    ExpectMatrix(*transform, expected);
    ExpectMatrix(*skeleton->GetSockets()[0].GetTransform(pose), expected);
    const auto world = glm::translate(glm::mat4(1), glm::vec3(10, 0, 0));
    ExpectMatrix(world * *transform, world * expected);
}

TEST(SkeletonSocketTest, FollowsAnimationAndIndependentPoses)
{
    const auto mesh = MakeSocketMesh();
    const auto& skeleton = mesh->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto sequence = Sequence::Create("Move", skeleton, 1, {{
        .BoneIndex = 1, .Translations = {{0, {0, 0, 0}}, {1, {0, 4, 0}}}
    }});
    const auto first = Animation::Create(skeleton, mesh->GetData().ReferencePose);
    const auto second = Animation::Create(skeleton, mesh->GetData().ReferencePose);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(first->Play(SequenceNode::Create(sequence), false));
    ASSERT_TRUE(first->Update(0.5f));
    ExpectMatrix(
        *grip->GetTransform(first->GetModelPose()),
        glm::translate(glm::mat4(1), glm::vec3(1, 2, 0))
    );
    ExpectMatrix(
        *grip->GetTransform(second->GetModelPose()),
        glm::translate(glm::mat4(1), glm::vec3(1, 0, 0))
    );
    first->Pause();
    ASSERT_TRUE(first->Update(0.25f));
    EXPECT_FLOAT_EQ((*grip->GetTransform(first->GetModelPose()))[3].y, 2);
    ASSERT_TRUE(first->Seek(0.75f));
    EXPECT_FLOAT_EQ((*grip->GetTransform(first->GetModelPose()))[3].y, 3);
    first->Stop();
    EXPECT_FLOAT_EQ((*grip->GetTransform(first->GetModelPose()))[3].y, 0);
}

TEST(SkeletonSocketTest, ReadsFinalMaskedAdditiveAndMontagePose)
{
    const auto mesh = MakeSocketMesh();
    const auto& skeleton = mesh->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto animation = Animation::Create(skeleton, mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    const auto mask = BoneMask::FromBone("Hand", skeleton, "Child");
    ASSERT_TRUE(animation->AddLayer("Action", mask, 0.5f));
    auto* layer = animation->GetLayer("Action");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    const auto sequence = Sequence::Create("Action", skeleton, 1, {{
        .BoneIndex = 1, .Translations = {{0, {0, 4, 0}}, {1, {0, 4, 0}}}
    }});
    const auto montage = AnimationMontage::Create({
        .Name = "Action", .Sequence = sequence, .BlendIn = 0, .BlendOut = 0
    });
    ASSERT_TRUE(layer->GetPlayer().PlayMontage(montage));
    ASSERT_TRUE(animation->Update(0.25f));
    const auto transform = grip->GetTransform(animation->GetModelPose());
    ASSERT_TRUE(transform);
    EXPECT_FLOAT_EQ((*transform)[3].y, 2);
    ExpectMatrix(
        *transform,
        animation->GetModelPose().Matrices[1] *
            skeleton->GetSockets()[0].RelativeTransform.ToMatrix()
    );
}

TEST(SkeletonSocketTest, UsesRootLockedPoseAndLeavesWorldMovementToCaller)
{
    const auto mesh = MakeSocketMesh();
    const auto& skeleton = mesh->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto animation = Animation::Create(skeleton, mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    const auto sequence = Sequence::Create("Move", skeleton, 1, {{
        .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}
    }});
    ASSERT_TRUE(animation->SetRootMotionEnabled(true));
    ASSERT_TRUE(animation->Play(SequenceNode::Create(sequence), false));
    ASSERT_TRUE(animation->Update(0.5f));
    const auto socket = grip->GetTransform(animation->GetModelPose());
    ASSERT_TRUE(socket);
    EXPECT_FLOAT_EQ((*socket)[3].x, 1);
    EXPECT_FLOAT_EQ((animation->GetRootMotionDelta().ToMatrix() * *socket)[3].x, 3);
}

TEST(SkeletonSocketTest, FailedPoseChangesKeepLastAcceptedSocket)
{
    const auto mesh = MakeSocketMesh();
    const auto& skeleton = mesh->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto animation = Animation::Create(skeleton, mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    const auto before = grip->GetTransform(animation->GetModelPose());
    ASSERT_TRUE(before);
    auto pose = animation->GetLocalPose();
    pose.LocalTransforms[1].Scale.x = 0;
    EXPECT_FALSE(animation->SetLocalPose(pose));
    EXPECT_FALSE(animation->Update(-1));
    ExpectMatrix(*grip->GetTransform(animation->GetModelPose()), *before);
}

TEST(SkeletonSocketTest, RejectsOverflowWhenCombiningFinitePoseAndOffset)
{
    auto data = MakeSocketMesh()->GetData();
    data.ReferencePose.LocalTransforms[0].Translation.x = std::numeric_limits<float>::max();
    auto socket = data.Skeleton->GetSockets()[0];
    socket.RelativeTransform.Translation.x = std::numeric_limits<float>::max();
    data.Skeleton = data.Skeleton->WithSockets({socket});
    const auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    const auto instance = SkeletalMeshInstance::Create(mesh);
    ASSERT_TRUE(instance);
    const auto& skeleton = mesh->GetSkeleton();
    const auto* grip = skeleton->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto& pose = instance->GetReferenceModelPose();
    EXPECT_FALSE(grip->GetTransform(pose));
    EXPECT_FALSE(skeleton->GetSockets()[0].GetTransform(pose));
}

TEST(SkeletonSocketTest, StandaloneSocketRejectsInvalidBoneAndOffset)
{
    SSkeletonSocket socket{.Name = "Grip", .BoneIndex = 1};
    const SModelPose pose{{glm::mat4(1)}};
    EXPECT_FALSE(socket.GetTransform(pose));
    socket.BoneIndex = 0;
    EXPECT_TRUE(socket.GetTransform(pose));
    socket.RelativeTransform.Scale.x = 0;
    EXPECT_FALSE(socket.GetTransform(pose));
    socket.RelativeTransform = {};
    socket.RelativeTransform.Rotation = {0, 0, 0, 0};
    EXPECT_FALSE(socket.GetTransform(pose));
    socket.RelativeTransform = {};
    socket.RelativeTransform.Translation.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(socket.GetTransform(pose));
}

TEST(SkeletonSocketTest, CachedSocketSurvivesCloningAndReadsNewPoses)
{
    const auto skeleton = Skeleton::Create("Rig", {{.Name = "Root"}}, {{.Name = "Grip"}});
    ASSERT_TRUE(skeleton);
    const auto* socket = skeleton->GetSocket("Grip");
    ASSERT_TRUE(socket);
    const auto clone = skeleton->WithSockets({});
    ASSERT_TRUE(clone);
    EXPECT_EQ(clone->GetSocket("Grip"), nullptr);
    EXPECT_EQ(skeleton->GetSocket("Grip"), socket);
    SModelPose pose{{glm::mat4(1)}};
    ASSERT_TRUE(socket->GetTransform(pose));
    pose.Matrices[0][3].x = 5;
    EXPECT_FLOAT_EQ((*socket->GetTransform(pose))[3].x, 5);
}
