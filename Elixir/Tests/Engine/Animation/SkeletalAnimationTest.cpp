#include <Engine/Animation/Pose.h>
#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationTestPlayback.h"
#include <gtest/gtest.h>

#include <Engine/Animation/Sequence.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>

#include <Engine/Mesh/MeshLoaderRegistry.h>
#include <Engine/Materials/DefaultMaterials.h>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <type_traits>

using namespace Elixir;

namespace
{
    Ref<Skeleton> MakeSkeleton()
    {
        return Skeleton::Create("Rig", {
            { .Name = "Root", .ReferenceTransform = { .Translation = { 2, 0, 0 } } },
            { .Name = "Hand", .ParentIndex = 0, .ReferenceTransform = { .Translation = { 0, 1, 0 } } },
        });
    }

    SSkeletalMeshData MakeData(const Ref<const Skeleton>& skeleton = MakeSkeleton())
    {
        SSkeletalMeshData data;
        data.Name = "Triangle";
        data.Skeleton = skeleton;
        data.BoneMap = { 1, 0 };
        data.InverseBindMatrices = {
            glm::translate(glm::mat4(1), glm::vec3(-2, -1, 0)),
            glm::translate(glm::mat4(1), glm::vec3(-2, 0, 0)),
        };
        data.Vertices.resize(3);
        data.Vertices[0].Position = { 2, 1, 0 };
        data.Vertices[1].Position = { 3, 1, 0 };
        data.Vertices[2].Position = { 2, 2, 0 };
        data.Indices = { 0, 1, 2 };
        data.Sections = {{ .IndexCount = 3 }};
        data.Materials = { Materials::CreateDefaultMaterials()[size_t(Materials::EMaterialUsage::Surface)] };
        return data;
    }

    Ref<Sequence> MakeSequence(const Ref<const Skeleton>& skeleton)
    {
        return Sequence::Create("RaiseHand", skeleton, 2, {
            { .BoneIndex = 1, .Translations = {{ 0, { 0, 1, 0 } }, { 2, { 0, 3, 0 } }} },
        });
    }

    void ExpectPosition(const glm::vec3& actual, const glm::vec3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, 1e-5f);
        EXPECT_NEAR(actual.y, expected.y, 1e-5f);
        EXPECT_NEAR(actual.z, expected.z, 1e-5f);
    }
}

TEST(SkeletonTest, RejectsInvalidHierarchiesAndTransforms)
{
    EXPECT_FALSE(Skeleton::Create("Empty", {}));
    EXPECT_FALSE(Skeleton::Create("Duplicate", {{ .Name = "Root" }, { .Name = "Root" }}));
    EXPECT_FALSE(Skeleton::Create("Cycle", {{ .Name = "Root", .ParentIndex = 0 }}));
    EXPECT_FALSE(Skeleton::Create("Forward", {{ .Name = "Root", .ParentIndex = 1 }, { .Name = "Child" }}));
    EXPECT_FALSE(Skeleton::Create("Negative", {{ .Name = "Root", .ParentIndex = -2 }}));
    EXPECT_FALSE(Skeleton::Create("Quaternion", {{ .Name = "Root", .ReferenceTransform = { .Rotation = { 0, 0, 0, 0 } } }}));
    EXPECT_FALSE(Skeleton::Create("Scale", {{ .Name = "Root", .ReferenceTransform = { .Scale = { 0, 1, 1 } } }}));
}

TEST(SkeletonTest, ComposesParentsAndPreservesOutputOnInvalidPose)
{
    auto skeleton = MakeSkeleton();
    ASSERT_TRUE(skeleton);
    EXPECT_EQ(skeleton->FindBone("Hand"), 1);
    EXPECT_EQ(skeleton->FindBone("Unknown"), -1);
    SLocalPose local;
    for (const auto& bone : skeleton->GetBones()) local.LocalTransforms.push_back(bone.ReferenceTransform);
    SModelPose model;
    ASSERT_TRUE(skeleton->BuildModelPose(local, model));
    ExpectPosition(glm::vec3(model.Matrices[1][3]), { 2, 1, 0 });
    local.LocalTransforms.pop_back();
    EXPECT_FALSE(skeleton->BuildModelPose(local, model));
    ASSERT_EQ(model.Matrices.size(), 2);
    ExpectPosition(glm::vec3(model.Matrices[1][3]), { 2, 1, 0 });
}

TEST(SkeletonTest, CompatibilityUsesNamesAndHierarchy)
{
    EXPECT_TRUE(MakeSkeleton()->IsCompatible(*MakeSkeleton()));
    auto other = Skeleton::Create("Other", {{ .Name = "Root" }, { .Name = "Hand" }});
    EXPECT_FALSE(MakeSkeleton()->IsCompatible(*other));
}

TEST(SequenceTest, SamplesChannelsAndRetainsMissingReferenceChannels)
{
    auto skeleton = MakeSkeleton();
    auto sequence = MakeSequence(skeleton);
    ASSERT_TRUE(sequence);
    SLocalPose pose;
    ASSERT_TRUE(sequence->Sample(1, pose));
    ExpectPosition(pose.LocalTransforms[0].Translation, { 2, 0, 0 });
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 2, 0 });
    ASSERT_TRUE(sequence->Sample(-3, pose));
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 1, 0 });
    ASSERT_TRUE(sequence->Sample(9, pose));
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 3, 0 });
    EXPECT_FALSE(sequence->Sample(std::numeric_limits<float>::quiet_NaN(), pose));
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 3, 0 });
}

TEST(SequenceTest, SupportsStepAndShortestPathQuaternionInterpolation)
{
    SBoneAnimationTrack track{ .BoneIndex = 1,
        .Translations = {{ 0, { 0, 1, 0 } }, { 2, { 0, 3, 0 } }},
        .Rotations = {{ 0, { 1, 0, 0, 0 } }, { 2, { -1, 0, 0, 0 } }},
        .TranslationInterpolation = EBoneInterpolation::Step };
    auto sequence = Sequence::Create("Step", MakeSkeleton(), 2, { track });
    ASSERT_TRUE(sequence);
    SLocalPose pose;
    ASSERT_TRUE(sequence->Sample(1, pose));
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 1, 0 });
    EXPECT_NEAR(std::abs(pose.LocalTransforms[1].Rotation.w), 1, 1e-5f);
    ASSERT_TRUE(sequence->Sample(2, pose));
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 3, 0 });
}

TEST(SequenceTest, RejectsInvalidKeysAndDuplicateTracks)
{
    auto skeleton = MakeSkeleton();
    EXPECT_FALSE(Sequence::Create("Bad", skeleton, -1, {}));
    EXPECT_FALSE(Sequence::Create("Bad", nullptr, 1, {}));
    EXPECT_FALSE(Sequence::Create("Bad", skeleton, 1, {{ .BoneIndex = 5 }}));
    EXPECT_FALSE(Sequence::Create("Bad", skeleton, 1, {{ .BoneIndex = 0 }, { .BoneIndex = 0 }}));
    EXPECT_FALSE(Sequence::Create("Bad", skeleton, 1,
        {{ .Translations = {{ 0, {} }, { 0, {} }} }}));
    EXPECT_FALSE(Sequence::Create("Bad", skeleton, 1,
        {{ .Translations = {{ 2, {} }} }}));
    EXPECT_FALSE(Sequence::Create("Bad", skeleton, 1,
        {{ .Rotations = {{ 0, { 0, 0, 0, 0 } }} }}));
}

TEST(SkeletalMeshTest, ReferencePoseAndReorderedPalettePreserveGeometry)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Position, { 2, 1, 0 });
    ExpectPosition(vertices[1].Position, { 3, 1, 0 });
    ASSERT_TRUE(meshInstance.Play(MakeNode(MakeSequence(mesh->GetSkeleton()))));
    ASSERT_TRUE(meshInstance.Update(1));
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Position, { 2, 2, 0 });
}

TEST(SkeletalMeshTest, NormalizesWeightsAndIgnoresUnusedJointIndices)
{
    auto data = MakeData();
    data.Vertices[0].Joints = { 0, 1, UINT32_MAX, UINT32_MAX };
    data.Vertices[0].Weights = { 2, 2, 0, 0 };
    auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    EXPECT_FLOAT_EQ(mesh->GetData().Vertices[0].Weights.x, 0.5f);
    EXPECT_EQ(mesh->GetData().Vertices[0].Joints.z, 0);
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    ASSERT_TRUE(meshInstance.Play(MakeNode(MakeSequence(mesh->GetSkeleton()))));
    ASSERT_TRUE(meshInstance.Update(1));
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Position, { 2, 1.5f, 0 });
}

TEST(SkeletalMeshTest, RejectsMalformedBindingsAndDrawRanges)
{
    auto data = MakeData();
    data.BoneMap[0] = 99;
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.InverseBindMatrices.pop_back();
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Vertices[0].Weights = glm::vec4(0);
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Vertices[0].Weights.x = -1;
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Vertices[0].Joints.x = 42;
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Sections[0].FirstIndex = UINT32_MAX;
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Sections[0].VertexOffset = 1;
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Indices[2] = 3;
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.Materials[0].reset();
    EXPECT_FALSE(SkeletalMesh::Create(data));
    data = MakeData(); data.InverseBindMatrices[0] = glm::mat4(0);
    EXPECT_FALSE(SkeletalMesh::Create(data));
}

TEST(SkeletalMeshTest, HandlesNormalsUnderNonuniformSkinScale)
{
    auto data = MakeData();
    const auto normal = glm::normalize(glm::vec3(1, 1, 0));
    data.Vertices[0].Normal = normal;
    auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    auto pose = meshInstance.GetLocalPose();
    pose.LocalTransforms[0].Scale = { 2, 1, 1 };
    ASSERT_TRUE(meshInstance.SetLocalPose(pose));
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Normal, glm::normalize(glm::vec3(0.5f, 1, 0)));
    EXPECT_NEAR(glm::dot(vertices[0].Normal, glm::vec3(vertices[0].Tangent)), 0, 1e-5f);
}

TEST(SkeletalMeshTest, InvalidModelPoseLeavesOutputUnchanged)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    std::vector<SStaticMeshVertex> vertices(1);
    vertices[0].Position = { 9, 8, 7 };
    EXPECT_FALSE(mesh->SkinVertices({}, vertices));
    auto model = meshInstance.GetModelPose(); model.Matrices[0] = glm::mat4(0);
    EXPECT_FALSE(mesh->SkinVertices(model, vertices));
    ASSERT_EQ(vertices.size(), 1);
    ExpectPosition(vertices[0].Position, { 9, 8, 7 });
}

TEST(AnimationTestCharacterTest, InstancesShareAssetsWithIndependentPlayback)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    auto sequence = MakeSequence(mesh->GetSkeleton());
    const auto firstHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(firstHandle);
    auto& first = *firstHandle;
    const auto secondHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    ASSERT_TRUE(first.Play(MakeNode(sequence)));
    ASSERT_TRUE(second.Play(MakeNode(sequence)));
    ASSERT_TRUE(first.Update(1));
    EXPECT_FLOAT_EQ(first.GetTime(), 1);
    EXPECT_FLOAT_EQ(second.GetTime(), 0);
    ExpectPosition(first.GetLocalPose().LocalTransforms[1].Translation, { 0, 2, 0 });
    ExpectPosition(second.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
}

TEST(AnimationTestCharacterTest, HandlesLoopingPauseSeekAndCompletion)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(meshInstance.Play(MakeNode(sequence)));
    ASSERT_TRUE(meshInstance.Update(5));
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 1);
    meshInstance.Pause();
    ASSERT_TRUE(meshInstance.Update(1));
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 1);
    ASSERT_TRUE(meshInstance.Seek(-0.5f));
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 1.5f);
    meshInstance.Resume();
    ASSERT_TRUE(meshInstance.Update(0.5f));
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 0);
    ASSERT_TRUE(meshInstance.Play(MakeNode(sequence), false));
    ASSERT_TRUE(meshInstance.Update(3));
    EXPECT_FALSE(meshInstance.IsPlaying());
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 2);
    ExpectPosition(meshInstance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3, 0 });
    meshInstance.Stop();
    ExpectPosition(meshInstance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    EXPECT_FALSE(meshInstance.Seek(0));
}

TEST(AnimationTestCharacterTest, FailedOperationsPreservePlaybackAndPose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    ASSERT_TRUE(meshInstance.Play(MakeNode(MakeSequence(mesh->GetSkeleton()))));
    ASSERT_TRUE(meshInstance.Update(1));
    EXPECT_FALSE(meshInstance.Play(MakeNode(Sequence::Create("Wrong", Skeleton::Create("Other", {{ .Name = "Other" }}), 1, {}))));
    EXPECT_FALSE(meshInstance.Update(-1));
    EXPECT_FALSE(meshInstance.Update(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(meshInstance.SetLocalPose({}));
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 1);
    EXPECT_TRUE(meshInstance.IsPlaying());
    EXPECT_EQ(meshInstance.GetMesh(), mesh);
}

TEST(AnimationTestCharacterTest, MeshSpecificReferencePoseSuppliesUnanimatedBones)
{
    auto data = MakeData();
    for (const auto& bone : data.Skeleton->GetBones()) data.ReferencePose.LocalTransforms.push_back(bone.ReferenceTransform);
    data.ReferencePose.LocalTransforms[0].Translation = { 10, 0, 0 };
    auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    ASSERT_TRUE(meshInstance.Play(MakeNode(MakeSequence(mesh->GetSkeleton()))));
    ASSERT_TRUE(meshInstance.Update(1));
    ExpectPosition(meshInstance.GetLocalPose().LocalTransforms[0].Translation, { 10, 0, 0 });
    meshInstance.Stop();
    ExpectPosition(meshInstance.GetLocalPose().LocalTransforms[0].Translation, { 10, 0, 0 });
}

TEST(AnimationTestCharacterTest, ZeroDurationSequenceProducesAStablePose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    auto sequence = Sequence::Create("Still", mesh->GetSkeleton(), 0,
        {{ .BoneIndex = 1, .Translations = {{ 0, { 0, 4, 0 } }} }});
    ASSERT_TRUE(meshInstance.Play(MakeNode(sequence)));
    EXPECT_FALSE(meshInstance.IsPlaying());
    meshInstance.Resume();
    EXPECT_FALSE(meshInstance.IsPlaying());
    ASSERT_TRUE(meshInstance.Update(100));
    ExpectPosition(meshInstance.GetLocalPose().LocalTransforms[1].Translation, { 0, 4, 0 });
}

TEST(SkeletalAnimationApiTest, SharedAssetsCannotBeReassigned)
{
    static_assert(!std::is_copy_constructible_v<Skeleton>);
    static_assert(!std::is_copy_assignable_v<Skeleton>);
    static_assert(!std::is_copy_assignable_v<SkeletalMesh>);
    static_assert(!std::is_copy_assignable_v<Sequence>);
}

TEST(SkeletalMeshTest, IndependentVertexTypesPreserveSurfaceAttributesDuringSkinning)
{
    static_assert(std::is_standard_layout_v<SStaticMeshVertex>);
    static_assert(std::is_standard_layout_v<SSkeletalMeshVertex>);
    static_assert(!std::is_base_of_v<SStaticMeshVertex, SSkeletalMeshVertex>);

    auto data = MakeData();
    data.Vertices[0].TexCoord = { 0.25f, 0.75f };
    data.Vertices[0].TexCoord1 = { 0.5f, 0.125f };
    data.Vertices[0].Tangent.w = -1.0f;
    auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    const auto meshInstanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(meshInstanceHandle);
    auto& meshInstance = *meshInstanceHandle;
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ASSERT_EQ(vertices.size(), 3);
    EXPECT_EQ(vertices[0].TexCoord, glm::vec2(0.25f, 0.75f));
    EXPECT_EQ(vertices[0].TexCoord1, glm::vec2(0.5f, 0.125f));
    EXPECT_EQ(vertices[0].Tangent, glm::vec4(1, 0, 0, -1));
    EXPECT_EQ(mesh->GetData().Vertices[0].Position, glm::vec3(2, 1, 0));
}

TEST(AnimationTestCharacterTest, CrossFadeAdvancesBothSequencesAndPreservesTheInitialPose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto target = Sequence::Create("LowerHand", mesh->GetSkeleton(), 4, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 9, 0 } }, { 4, { 0, 13, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(MakeSequence(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_TRUE(instance.Play(MakeNode(target), 2.0f));
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.0f);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.5f, 0 });
    ASSERT_TRUE(instance.Update(0.5f));
    // Source at 1 second: y=2. Target at 0.5 seconds: y=9.5. Weight: 0.25.
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3.875f, 0 });
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 2.0f);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 11, 0 });
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 11.5f, 0 });
}

TEST(AnimationTestCharacterTest, CrossFadeBlendsLocalTranslationScaleAndRotationBeforeHierarchy)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0, 0, 1));
    auto target = Sequence::Create("Turn", mesh->GetSkeleton(), 2, {
        { .BoneIndex = 0, .Translations = {{ 0, { 6, 0, 0 } }},
          .Rotations = {{ 0, -rotation }}, .Scales = {{ 0, { 3, 3, 3 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(target), 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[0].Translation, { 4, 0, 0 });
    ExpectPosition(instance.GetLocalPose().LocalTransforms[0].Scale, { 2, 2, 2 });
    const auto expectedRotation = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1));
    EXPECT_NEAR(std::abs(glm::dot(instance.GetLocalPose().LocalTransforms[0].Rotation, expectedRotation)),
        1.0f, 1e-5f);
    // The unanimated child retains its local reference pose and inherits the blended root.
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[1][3]), { 4 - std::sqrt(3.0f), 1, 0 });
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(instance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Position, { 4 - std::sqrt(3.0f), 1, 0 });
}

TEST(AnimationTestCharacterTest, InterruptedCrossFadeStartsFromTheVisiblePose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto first = Sequence::Create("First", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 5, 0 } }} },
    });
    auto second = Sequence::Create("Second", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 9, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(first), 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Play(MakeNode(second), 2.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Update(0.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 6, 0 });
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 9, 0 });
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FALSE(instance.IsPlaying());
}

TEST(AnimationTestCharacterTest, PauseAndResumeControlTransitionTime)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto target = Sequence::Create("Still", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 5, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(target), 2.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    instance.Pause();
    ASSERT_TRUE(instance.Update(10.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 2, 0 });
    EXPECT_TRUE(instance.IsTransitioning());
    instance.Resume();
    EXPECT_TRUE(instance.IsPlaying());
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Update(10.0f));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 5, 0 });
}

TEST(AnimationTestCharacterTest, NonloopingTargetFinishesOnlyAfterTheFade)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeSequence(mesh->GetSkeleton())), 4.0f, false));
    ASSERT_TRUE(instance.Update(2.0f));
    EXPECT_TRUE(instance.IsPlaying());
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 2.0f);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 2, 0 });
    ASSERT_TRUE(instance.Update(2.0f));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3, 0 });
}

TEST(AnimationTestCharacterTest, CrossFadeWrapsSourceAndTargetIndependently)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto target = Sequence::Create("Target", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 5, 0 } }, { 1, { 0, 9, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(MakeSequence(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.Update(1.5f));
    ASSERT_TRUE(instance.Play(MakeNode(target), 3.0f));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.5f);
    // Source wraps to t=1 (y=2); target wraps to t=0.5 (y=7).
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 4.5f, 0 });
}

TEST(AnimationTestCharacterTest, CrossFadeClampsNonloopingSourceAndSupportsImmediateSwitch)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto target = Sequence::Create("Target", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 9, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(MakeSequence(mesh->GetSkeleton())), false));
    ASSERT_TRUE(instance.Update(1.5f));
    ASSERT_TRUE(instance.Play(MakeNode(target), 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 6, 0 });
    ASSERT_TRUE(instance.Play(MakeNode(target), 0.0f));
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FALSE(instance.IsPlaying());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 9, 0 });
}

TEST(AnimationTestCharacterTest, SeekStopAndManualPoseCancelTransitions)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(instance.Play(MakeNode(sequence), 2.0f));
    ASSERT_TRUE(instance.Seek(1.0f));
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 2, 0 });
    ASSERT_TRUE(instance.Play(MakeNode(sequence), 2.0f));
    instance.Stop();
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    auto pose = instance.GetLocalPose();
    pose.LocalTransforms[1].Translation = { 0, 7, 0 };
    ASSERT_TRUE(instance.Play(MakeNode(sequence), 2.0f));
    ASSERT_TRUE(instance.SetLocalPose(pose));
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FALSE(instance.IsPlaying());
    ASSERT_TRUE(instance.Play(MakeNode(sequence), 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 4.5f, 0 });
    const auto replacementHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(replacementHandle);
    auto& replacement = *replacementHandle;
    EXPECT_FALSE(replacement.IsTransitioning());
    ExpectPosition(replacement.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    EXPECT_TRUE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 4.5f, 0 });
}

TEST(AnimationTestCharacterTest, InvalidFadeRequestsPreserveAnActiveTransition)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(instance.Play(MakeNode(sequence), 2.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto before = instance.GetLocalPose().LocalTransforms[1].Translation;
    EXPECT_FALSE(instance.Play(MakeNode(sequence), -1.0f));
    EXPECT_FALSE(instance.Play(MakeNode(sequence), std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(instance.Play(MakeNode(sequence), std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(instance.Play(MakeNode(nullptr), 1.0f));
    auto incompatible = Sequence::Create("Other",
        Skeleton::Create("Other", {{ .Name = "Other" }}), 0, {});
    EXPECT_FALSE(instance.Play(MakeNode(incompatible), 1.0f));
    EXPECT_FALSE(instance.Update(-1.0f));
    EXPECT_FALSE(instance.Seek(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.5f);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, before);
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.5f, 0 });
}

TEST(AnimationTestCharacterTest, SingularBlendedScalePreservesPoseAndTransitionProgress)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto target = Sequence::Create("Mirror", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 0, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(target), 2.0f));
    EXPECT_FALSE(instance.Update(1.0f));
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.0f);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[0].Scale, { 1, 1, 1 });
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[0].Scale, { 0.5f, 1, 1 });
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[0].Scale, { -1, 1, 1 });
}

TEST(AnimationTestCharacterTest, CrossFadeUsesMeshSpecificDefaultsAndIndependentInstanceState)
{
    auto data = MakeData();
    for (const auto& bone : data.Skeleton->GetBones())
        data.ReferencePose.LocalTransforms.push_back(bone.ReferenceTransform);
    data.ReferencePose.LocalTransforms[0].Translation = { 10, 0, 0 };
    auto mesh = SkeletalMesh::Create(std::move(data));
    const auto firstHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(firstHandle);
    auto& first = *firstHandle;
    const auto secondHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(first.Play(MakeNode(sequence), 2.0f));
    ASSERT_TRUE(second.Play(MakeNode(sequence), 4.0f));
    ASSERT_TRUE(first.Update(1.0f));
    ASSERT_TRUE(second.Update(1.0f));
    ExpectPosition(first.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.5f, 0 });
    ExpectPosition(second.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.25f, 0 });
    ExpectPosition(first.GetLocalPose().LocalTransforms[0].Translation, { 10, 0, 0 });
    ExpectPosition(second.GetLocalPose().LocalTransforms[0].Translation, { 10, 0, 0 });
}

TEST(AnimationTestCharacterTest, MeshOwnershipIsFixedAtConstruction)
{
    static_assert(!std::is_copy_constructible_v<Animation>);
    static_assert(!std::is_copy_assignable_v<Animation>);
    static_assert(!std::is_default_constructible_v<AnimationTestCharacter>);
    static_assert(!std::is_copy_assignable_v<AnimationTestCharacter>);
    static_assert(!std::is_move_assignable_v<AnimationTestCharacter>);
    static_assert(std::is_same_v<decltype(std::declval<AnimationTestCharacter&>().GetMesh()),
        const Ref<const SkeletalMesh>&>);

    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    const std::weak_ptr<const SkeletalMesh> weak = mesh;
    {
        const auto instanceHandle = AnimationTestCharacter::Create(mesh);
        ASSERT_TRUE(instanceHandle);
        auto& instance = *instanceHandle;
        mesh.reset();
        EXPECT_FALSE(weak.expired());
        ASSERT_TRUE(instance.Play(MakeNode(MakeSequence(instance.GetMesh()->GetSkeleton()))));
        ASSERT_TRUE(instance.Update(1));
        instance.Stop();
        EXPECT_EQ(instance.GetMesh(), weak.lock());
        ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    }
    EXPECT_TRUE(weak.expired());
}

TEST(AnimationTestCharacterTest, RejectsNullMeshConstruction)
{
    EXPECT_FALSE(AnimationTestCharacter::Create(nullptr));
}


TEST(AnimationNotifyTest, ValidatesAndStablySortsAuthoredEvents)
{
    auto clip = MakeSequence(MakeSkeleton());
    for (const auto time : { -1.0f, 3.0f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN() })
        EXPECT_FALSE(clip->WithNotifies({{ "Hit", time }}));
    EXPECT_FALSE(clip->WithNotifies({{ "", 1 }}));
    EXPECT_FALSE(Sequence::Create("Static", MakeSkeleton(), 0, {}, {}, {{ "Hit", 0 }}));
    auto marked = clip->WithSyncMarkers({{ "Left", 0.5f }, { "Right", 1.5f }});
    auto events = marked->WithNotifies({{ "End", 2 }, { "Step", 0.5f, 0, std::string("Left") },
        { "Step", 0.5f, 0, std::string("Right") }});
    ASSERT_TRUE(events);
    EXPECT_TRUE(clip->GetNotifies().empty());
    EXPECT_EQ(events->GetSyncMarkers().size(), 2);
    ASSERT_NE(events->GetNotifies()[0].GetPayload<std::string>(), nullptr);
    ASSERT_NE(events->GetNotifies()[1].GetPayload<std::string>(), nullptr);
    EXPECT_EQ(*events->GetNotifies()[0].GetPayload<std::string>(), "Left");
    EXPECT_EQ(*events->GetNotifies()[1].GetPayload<std::string>(), "Right");
    EXPECT_EQ(events->WithSyncMarkers({})->GetNotifies().size(), 3);
    SLocalPose pose;
    ASSERT_TRUE(events->Sample(1, pose));
    ExpectPosition(pose.LocalTransforms[1].Translation, { 0, 2, 0 });
}

TEST(AnimationNotifyTest, OrdersSeveralLoopsAndBoundaryEvents)
{
    auto clip = MakeSequence(MakeSkeleton())->WithNotifies({
        { "Start", 0 }, { "Middle", 1 }, { "End", 2 },
    });
    std::vector<SAnimationNotifyInterval> events;
    ASSERT_TRUE(clip->CollectNotifyIntervals(0, 2, true, events, nullptr, true));
    ASSERT_EQ(events.size(), 7);
    const std::array<size_t, 7> expected{ 0, 1, 2, 0, 1, 2, 0 };
    for (size_t i = 0; i < events.size(); ++i)
    {
        EXPECT_EQ(events[i].NotifyIndex, expected[i]);
        if (i) EXPECT_LE(events[i - 1].StartPhaseOffset, events[i].StartPhaseOffset);
    }
    ASSERT_TRUE(clip->CollectNotifyIntervals(0.5, 1, false, events));
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front().NotifyIndex, 2);
    ASSERT_TRUE(clip->CollectNotifyIntervals(1, 2, false, events));
    EXPECT_TRUE(events.empty());
}

TEST(AnimationNotifyTest, CapacityAndInvalidInputPreserveOutput)
{
    auto clip = MakeSequence(MakeSkeleton())->WithNotifies({{ "Step", 1 }});
    std::vector<SAnimationNotifyInterval> events{{ 42, 3 }};
    EXPECT_FALSE(clip->CollectNotifyIntervals(0, 1e30, true, events));
    EXPECT_FALSE(clip->CollectNotifyIntervals(0.5, 0.4, true, events));
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front().NotifyIndex, 42);
    ASSERT_TRUE(clip->CollectNotifyIntervals(0, 4096, true, events));
    EXPECT_EQ(events.size(), Sequence::MaxNotifyEventsPerUpdate);
}

TEST(AnimationNotifyTest, PlaybackPauseSeekAndNonloopingEndAreSilentOrEmitOnce)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    auto clip = MakeSequence(mesh->GetSkeleton())->WithNotifies({
        { "Start", 0 }, { "Step", 1, 0, std::string("Left") }, { "End", 2 },
    });
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip), false));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(1));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyEvents()[0].GetName(), "Start");
    EXPECT_DOUBLE_EQ(instance.GetNotifyEvents()[1].TimeOffset, 1);
    instance.Pause();
    ASSERT_TRUE(instance.Update(5));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    instance.Resume();
    ASSERT_TRUE(instance.Seek(0));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Seek(1.5f));
    ASSERT_TRUE(instance.Update(5));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    auto retained = instance.GetNotifyEvents().front();
    EXPECT_EQ(retained.GetName(), "End");
    EXPECT_DOUBLE_EQ(retained.TimeOffset, 0.5);
    ASSERT_TRUE(instance.Update(5));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    instance.Stop();
    clip.reset();
    EXPECT_EQ(retained.GetName(), "End");
}

TEST(AnimationNotifyTest, FailedUpdatePreservesPlaybackAndPreviousBatch)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeSequence(mesh->GetSkeleton())->WithNotifies({{ "Step", 1 }}))));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(instance.Update(10000));
    EXPECT_FLOAT_EQ(instance.GetTime(), 1);
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_FALSE(instance.Update(-1));
    EXPECT_FLOAT_EQ(instance.GetTime(), 1);
}

TEST(AnimationNotifyTest, FadesAndInterruptedFadesEmitOnlyTargetEvents)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto make = [&](const char* name)
    {
        return MakeSequence(mesh->GetSkeleton())->WithNotifies({{ name, 0.5f }});
    };
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(make("Outgoing"))));
    ASSERT_TRUE(instance.Play(MakeNode(make("Target")), 2.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().GetName(), "Target");
    ASSERT_TRUE(instance.Play(MakeNode(make("Interrupted")), 2.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().GetName(), "Interrupted");
}

TEST(AnimationNotifyTest, BlendUsesDominantSourceAndIdleSuppressesEvents)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    auto idle = MakeSequence(mesh->GetSkeleton());
    auto move = idle->WithNotifies({{ "Step", 0.5f }});
    auto space = BlendSpace1D::Create("Movement", {{ 0, idle }, { 1, move }});
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), { 0.25f }));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.SetInputs({ 0.75f }));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(2));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().GetName(), "Step");
    auto retained = instance.GetNotifyEvents().front();
    instance.Stop();
    space.reset();
    move.reset();
    EXPECT_EQ(retained.GetName(), "Step");
}

TEST(AnimationNotifyTest, MarkerMappingFindsContactsAcrossTheLocalCycleBoundary)
{
    auto base = MakeSequence(MakeSkeleton());
    auto reference = base->WithSyncMarkers({{ "Left", 0.5f }, { "Right", 1.5f }});
    auto target = base->WithSyncMarkers({{ "Right", 0.25f }, { "Left", 1.25f }})
        ->WithNotifies({{ "LeftStep", 1.25f }, { "RightStep", 0.25f }, { "Wrap", 0 }});
    std::vector<SAnimationNotifyInterval> events;
    ASSERT_TRUE(target->CollectNotifyIntervals(0, 1, true, events, reference.get()));
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(target->GetNotifies()[events[0].NotifyIndex].Name, "LeftStep");
    EXPECT_DOUBLE_EQ(events[0].StartPhaseOffset, 0.25);
    EXPECT_EQ(target->GetNotifies()[events[1].NotifyIndex].Name, "Wrap");
    EXPECT_DOUBLE_EQ(events[1].StartPhaseOffset, 0.625);
    EXPECT_EQ(target->GetNotifies()[events[2].NotifyIndex].Name, "RightStep");
    EXPECT_DOUBLE_EQ(events[2].StartPhaseOffset, 0.75);
}


TEST(AnimationNotifyTest, DominantSourceChangesKeepAuthoredMarkerEventsOnTheSamePhase)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    auto base = MakeSequence(mesh->GetSkeleton());
    auto first = base->WithSyncMarkers({{ "Left", 0.3217778f }, { "Right", 0.8622223f }})
        ->WithNotifies({{ "Step", 0.3217778f, 0, std::string("Left") }});
    auto second = base->WithSyncMarkers({{ "Left", 0.1146667f }, { "Right", 0.5146667f }})
        ->WithNotifies({{ "Step", 0.1146667f, 0, std::string("Left") }});
    auto space = BlendSpace2D::Create("Movement", {
        {{ 0, 0 }, first }, {{ 1, 0 }, second }, {{ 0, 1 }, base },
    });
    std::vector<SAnimationNotifyInterval> expected;
    std::vector<SAnimationNotifyInterval> actual;
    const auto* reference = space->GetSynchronizationSequence().get();
    ASSERT_TRUE(first->CollectNotifyIntervals(0, 1, true, expected, reference));
    ASSERT_TRUE(second->CollectNotifyIntervals(0, 1, true, actual, reference));
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(actual.size(), 1);
    EXPECT_DOUBLE_EQ(actual[0].StartPhaseOffset, expected[0].StartPhaseOffset);
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), { 0, 0 }));
    ASSERT_TRUE(instance.Update(0.4f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(instance.SetInputs({ 1, 0 }));
    ASSERT_TRUE(instance.Update(0.01f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(2));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    const auto* payload = instance.GetNotifyEvents().front().GetPayload<std::string>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(*payload, "Left");
}


namespace
{
    enum class ENotifyTestFoot { Left, Right };

    struct SNotifyTestImpact
    {
        int Damage;
        glm::vec3 Direction;
        Ref<const std::string> Effect;
    };
}

TEST(AnimationNotifyTest, SupportsMixedPayloadTypesAndSafeMismatches)
{
    const auto effect = CreateRef<const std::string>("Impact");
    auto clip = MakeSequence(MakeSkeleton())->WithNotifies({
        { "Footstep", 0.25f, 0, ENotifyTestFoot::Right },
        { "Hit", 0.5f, 0, SNotifyTestImpact{ 25, { 0, 1, 0 }, effect } },
        { "Scale", 0.75f, 0, 0.5f },
        { "Signal", 1 },
    });
    ASSERT_TRUE(clip);
    const auto& notifies = clip->GetNotifies();
    static_assert(std::is_same_v<decltype(notifies[0].GetPayload<ENotifyTestFoot>()),
        const ENotifyTestFoot*>);
    ASSERT_NE(notifies[0].GetPayload<ENotifyTestFoot>(), nullptr);
    EXPECT_EQ(*notifies[0].GetPayload<ENotifyTestFoot>(), ENotifyTestFoot::Right);
    const auto* impact = notifies[1].GetPayload<SNotifyTestImpact>();
    ASSERT_NE(impact, nullptr);
    EXPECT_EQ(impact->Damage, 25);
    ExpectPosition(impact->Direction, { 0, 1, 0 });
    EXPECT_EQ(impact->Effect, effect);
    ASSERT_NE(notifies[2].GetPayload<float>(), nullptr);
    EXPECT_FLOAT_EQ(*notifies[2].GetPayload<float>(), 0.5f);
    EXPECT_EQ(notifies[2].GetPayload<double>(), nullptr);
    EXPECT_EQ(notifies[0].GetPayload<SNotifyTestImpact>(), nullptr);
    EXPECT_EQ(notifies[3].GetPayload<SNotifyTestImpact>(), nullptr);
    EXPECT_FALSE(notifies[3].Payload.has_value());
}

TEST(AnimationNotifyTest, CopiesAuthoredValuesAndPreservesTypesThroughAssetClones)
{
    SNotifyTestImpact original{ 10, { 1, 0, 0 }, CreateRef<const std::string>("Impact") };
    std::vector<SAnimationNotify> authored{{ "Hit", 1, 0, original }};
    auto clip = MakeSequence(MakeSkeleton())->WithNotifies(authored);
    ASSERT_TRUE(clip);
    original.Damage = 50;
    std::any_cast<SNotifyTestImpact>(&authored.front().Payload)->Damage = 99;
    auto marked = clip->WithSyncMarkers({{ "Left", 0.5f }, { "Right", 1.5f }});
    ASSERT_TRUE(marked);
    auto cloned = marked->WithNotifies(marked->GetNotifies());
    ASSERT_TRUE(cloned);
    EXPECT_EQ(cloned->GetSyncMarkers().size(), 2);
    for (const auto& asset : { clip, marked, cloned })
    {
        const auto* value = asset->GetNotifies().front().GetPayload<SNotifyTestImpact>();
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(value->Damage, 10);
        EXPECT_EQ(*value->Effect, "Impact");
    }
}

TEST(AnimationNotifyTest, RetainedBlendEventsKeepCustomPayloadResourcesAlive)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    std::weak_ptr<const std::string> effectLifetime;
    SAnimationNotifyEvent retained;
    {
        auto effect = CreateRef<const std::string>("Impact");
        effectLifetime = effect;
        auto clip = MakeSequence(mesh->GetSkeleton())->WithNotifies({
            { "Hit", 0.5f, 0, SNotifyTestImpact{ 42, { 0, 0, 1 }, effect } },
        });
        auto space = BlendSpace1D::Create("Action", {{ 0, clip }});
        ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), { 0 }));
        ASSERT_TRUE(instance.Update(0.5f));
        ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
        retained = instance.GetNotifyEvents().front();
        instance.Stop();
    }
    EXPECT_FALSE(effectLifetime.expired());
    const auto* value = retained.GetPayload<SNotifyTestImpact>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->Damage, 42);
    EXPECT_EQ(*value->Effect, "Impact");
    retained = {};
    EXPECT_TRUE(effectLifetime.expired());
}

namespace
{
    Ref<Skeleton> MakeLayerSkeleton()
    {
        return Skeleton::Create("LayerRig", {
            { .Name = "Root" },
            { .Name = "Spine", .ParentIndex = 0,
                .ReferenceTransform = { .Translation = { 0, 1, 0 } } },
            { .Name = "Hand", .ParentIndex = 1,
                .ReferenceTransform = { .Translation = { 0, 1, 0 } } },
            { .Name = "Leg", .ParentIndex = 0,
                .ReferenceTransform = { .Translation = { 0, -1, 0 } } },
        });
    }

    Ref<Sequence> MakeLayerBase(const Ref<const Skeleton>& skeleton)
    {
        return Sequence::Create("Walk", skeleton, 2, {
            { .BoneIndex = 1, .Translations = {{ 0, { 0, 1, 0 } }, { 2, { 0, 3, 0 } }} },
            { .BoneIndex = 3, .Translations = {{ 0, { 0, -1, 0 } }, { 2, { 0, -3, 0 } }} },
        });
    }

    Ref<Sequence> MakeLayerAction(const Ref<const Skeleton>& skeleton)
    {
        return Sequence::Create("Action", skeleton, 1, {
            { .BoneIndex = 1, .Translations = {{ 0, { 0, 10, 0 } }, { 1, { 0, 20, 0 } }} },
            { .BoneIndex = 2, .Translations = {{ 0, { 2, 1, 0 } }} },
            { .BoneIndex = 3, .Translations = {{ 0, { 0, 50, 0 } }} },
        });
    }
}

TEST(BoneMaskTest, ValidatesWeightsAndSelectsOnlyTheRequestedSubtree)
{
    static_assert(!std::is_copy_constructible_v<BoneMask>);
    static_assert(!std::is_copy_assignable_v<BoneMask>);
    auto skeleton = MakeLayerSkeleton();
    EXPECT_FALSE(BoneMask::Create("Empty", skeleton, {}));
    EXPECT_FALSE(BoneMask::Create("Bad", nullptr, {}));
    EXPECT_FALSE(BoneMask::FromBone("Bad", skeleton, "Missing"));
    for (const auto weight : { -0.1f, 1.1f, std::numeric_limits<float>::quiet_NaN() })
        EXPECT_FALSE(BoneMask::FromBone("Bad", skeleton, "Spine", weight));
    auto mask = BoneMask::FromBone("Upper", skeleton, "Spine", 0.5f);
    ASSERT_TRUE(mask);
    EXPECT_EQ(mask->GetWeights(), (std::vector<float>{ 0, 0.5f, 0.5f, 0 }));
    EXPECT_TRUE(mask->HasInfluence());
    auto single = BoneMask::FromBone("Single", skeleton, "Spine", 1, false);
    ASSERT_TRUE(single);
    EXPECT_EQ(single->GetWeights(), (std::vector<float>{ 0, 1, 0, 0 }));
    auto empty = BoneMask::FromBone("Muted", skeleton, "Root", 0);
    ASSERT_TRUE(empty);
    EXPECT_FALSE(empty->HasInfluence());
}

TEST(AnimationLayerTest, OverridesUpperBodyWhileLegsFollowBasePlayback)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* layer = instance.GetLayer("Upper");
    ASSERT_NE(layer, nullptr);
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(MakeLayerAction(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 15, 0 });
    ExpectPosition(instance.GetLocalPose().LocalTransforms[2].Translation, { 2, 1, 0 });
    ExpectPosition(instance.GetLocalPose().LocalTransforms[3].Translation, { 0, -1.5f, 0 });
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetNormalizedTime(), 0.5f);
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), { 2, 16, 0 });
}

TEST(AnimationLayerTest, MultipliesMaskAndLayerWeightsAndPreservesUnassignedLayers)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.AddLayer("Upper",
        BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine", 0.5f), 0.5f));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.5f, 0 });
    ASSERT_TRUE(instance.GetLayer("Upper")->GetPlayer().Play(
        MakeNode(MakeLayerAction(mesh->GetSkeleton()))
    ));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3.625f, 0 });
    EXPECT_EQ(instance.GetLayerWeight("Upper"), 0.5f);
    EXPECT_FALSE(instance.SetLayerWeight("Upper", -1));
    EXPECT_FALSE(instance.SetLayerWeight("Upper", std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(instance.GetLayerWeight("Missing"));
    EXPECT_FALSE(instance.RemoveLayer("Missing"));
    EXPECT_FALSE(instance.AddLayer("Upper",
        BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    EXPECT_FALSE(instance.AddLayer("Bad", BoneMask::FromBone("Other", MakeSkeleton(), "Root")));
    EXPECT_FALSE(instance.AddLayer("Bad", nullptr));
    const AnimationTestCharacter& readOnly = instance;
    EXPECT_EQ(readOnly.GetLayer("Upper"), instance.GetLayer("Upper"));
    EXPECT_EQ(readOnly.GetLayer("Missing"), nullptr);
}

TEST(AnimationLayerTest, AppliesLayersInOrderAndRestoresTheUnderlyingPoseWhenRemoved)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto mask = BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine");
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.AddLayer("First", mask));
    auto* first = instance.GetLayer("First");
    ASSERT_TRUE(first->GetPlayer().Play(MakeNode(MakeLayerAction(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.AddLayer("Second", mask, 0.5f));
    EXPECT_EQ(instance.GetLayer("First"), first);
    auto* second = instance.GetLayer("Second");
    ASSERT_TRUE(second->GetPlayer().Play(
        MakeNode(MakeLayerAction(mesh->GetSkeleton())),
        false
    ));
    ASSERT_TRUE(second->GetPlayer().Seek(1));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 15, 0 });
    ASSERT_TRUE(instance.RemoveLayer("Second"));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 10, 0 });
    instance.ClearLayers();
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
}

TEST(AnimationLayerTest, SupportsIndependentPauseSeekCompletionAndManualPoses)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* layer = instance.GetLayer("Upper");
    ASSERT_TRUE(layer->GetPlayer().Play(
        MakeNode(MakeLayerAction(mesh->GetSkeleton())),
        false
    ));
    EXPECT_TRUE(instance.IsPlaying());
    ASSERT_TRUE(instance.Update(0.25f));
    layer->GetPlayer().Pause();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetTime(), 0.25f);
    layer->GetPlayer().Resume();
    instance.Pause();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetTime(), 0.25f);
    instance.Resume();
    ASSERT_TRUE(instance.Update(2));
    EXPECT_FALSE(layer->GetPlayer().IsPlaying());
    EXPECT_FALSE(instance.IsPlaying());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 20, 0 });
    ASSERT_TRUE(layer->GetPlayer().Seek(0.5f));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 15, 0 });
    auto pose = layer->GetPlayer().GetLocalPose();
    pose.LocalTransforms[1].Translation.y = 30;
    ASSERT_TRUE(layer->GetPlayer().SetLocalPose(pose));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 30, 0 });
    layer->GetPlayer().Stop();
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    instance.Stop();
    EXPECT_EQ(instance.GetLayer("Upper"), nullptr);
}

TEST(AnimationLayerTest, BlendSpacesAndInterruptedTransitionsUseIndependentClocks)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto base = MakeLayerBase(mesh->GetSkeleton());
    auto action = MakeLayerAction(mesh->GetSkeleton());
    auto space = BlendSpace1D::Create("Actions", {{ 0, base }, { 1, action }});
    ASSERT_TRUE(instance.Play(MakeNode(base)));
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* layer = instance.GetLayer("Upper");
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(space), { 1 }));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(base), 1.0f));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(layer->GetPlayer().IsTransitioning());
    const auto before = instance.GetLocalPose().LocalTransforms[1].Translation;
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(action), 1.0f));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, before);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.75f);
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetTime(), 0.25f);
    ASSERT_TRUE(instance.Play(MakeNode(base), 0.5f));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetTime(), 0.25f);
}

TEST(AnimationLayerTest, MergesEventsChronologicallyAndRetainsLayerIdentityAndPayload)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton())->WithNotifies({{ "Step", 0.5f }}))));
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* layer = instance.GetLayer("Upper");
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(
        MakeLayerAction(mesh->GetSkeleton())->WithNotifies({
        { "Start", 0 }, { "Hit", 0.25f, 0, 42 }, { "End", 1 },
    }))));
    ASSERT_TRUE(instance.Update(0.5f));
    auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].GetName(), "Start");
    EXPECT_EQ(events[0].LayerName, "Upper");
    EXPECT_EQ(events[1].GetName(), "Hit");
    EXPECT_DOUBLE_EQ(events[1].TimeOffset, 0.25);
    ASSERT_NE(events[1].GetPayload<int>(), nullptr);
    EXPECT_EQ(*events[1].GetPayload<int>(), 42);
    EXPECT_EQ(events[2].GetName(), "Step");
    EXPECT_EQ(events[2].LayerName, "Base");
    auto retained = events[1];
    ASSERT_TRUE(instance.RemoveLayer("Upper"));
    EXPECT_EQ(retained.LayerName, "Upper");
    EXPECT_EQ(*retained.GetPayload<int>(), 42);
}

TEST(AnimationLayerTest, MutedLayersAndEmptyMasksAdvanceWithoutEmittingEvents)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto clip = MakeLayerAction(mesh->GetSkeleton())->WithNotifies({{ "Hit", 0.25f }});
    ASSERT_TRUE(instance.AddLayer("Muted", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine"), 0));
    ASSERT_TRUE(instance.AddLayer("Empty", BoneMask::FromBone("Empty", mesh->GetSkeleton(), "Root", 0)));
    ASSERT_TRUE(instance.GetLayer("Muted")->GetPlayer().Play(MakeNode(clip)));
    ASSERT_TRUE(instance.GetLayer("Empty")->GetPlayer().Play(MakeNode(clip)));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    EXPECT_FLOAT_EQ(instance.GetLayer("Muted")->GetPlayer().GetTime(), 0.5f);
    ASSERT_TRUE(instance.SetLayerWeight("Muted", 1));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().LayerName, "Muted");
}

TEST(AnimationLayerTest, FailedCompositionPreservesEveryClockPoseAndEventBatch)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto base = MakeLayerBase(mesh->GetSkeleton())->WithNotifies({{ "Step", 0 }});
    auto flipped = Sequence::Create("Flip", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeNode(base)));
    ASSERT_TRUE(instance.Update(0.125f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine"), 0.5f));
    ASSERT_TRUE(instance.GetLayer("Upper")->GetPlayer().Play(MakeNode(flipped)));
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.125f);
    EXPECT_FLOAT_EQ(instance.GetLayer("Upper")->GetPlayer().GetTime(), 0);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.125f, 0 });
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(instance.SetLayerWeight("Upper", 1));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.375f);
    EXPECT_FLOAT_EQ(instance.GetLayer("Upper")->GetPlayer().GetTime(), 0.25f);
}

TEST(AnimationLayerTest, CombinedNotifyLimitRollsBackEveryPlayer)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto clip = MakeLayerAction(mesh->GetSkeleton())->WithNotifies({{ "Hit", 0.25f }});
    ASSERT_TRUE(instance.Play(MakeNode(clip)));
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    ASSERT_TRUE(instance.GetLayer("Upper")->GetPlayer().Play(MakeNode(clip)));
    EXPECT_FALSE(instance.Update(3000));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_FLOAT_EQ(instance.GetLayer("Upper")->GetPlayer().GetTime(), 0);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyEvents()[0].GetName(), "Hit");
    EXPECT_EQ(instance.GetNotifyEvents()[0].LayerName, "Base");
    EXPECT_EQ(instance.GetNotifyEvents()[1].LayerName, "Upper");
}


TEST(AnimationLayerTest, NonloopingEndNotifySurvivesFloatRoundingNearCompletion)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* layer = instance.GetLayer("Upper");
    const auto action = MakeLayerAction(mesh->GetSkeleton())->WithNotifies({{ "End", 1 }});
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(action), false));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_TRUE(instance.Update(std::nextafter(0.5f, 0.0f)));
    EXPECT_TRUE(layer->GetPlayer().IsPlaying());
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.0001f));
    EXPECT_FALSE(layer->GetPlayer().IsPlaying());
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().GetName(), "End");
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST(AnimationLayerTest, BaseLayerHasFixedIdentityMaskAndLifetime)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& base = instance.GetBaseLayer();
    static_assert(std::is_same_v<decltype(base), AnimationLayer&>);
    static_assert(!std::is_copy_assignable_v<AnimationLayer>);
    EXPECT_EQ(instance.GetLayerCount(), 1);
    EXPECT_EQ(instance.GetLayer("Base"), &base);
    EXPECT_EQ(base.GetName(), "Base");
    EXPECT_FLOAT_EQ(base.GetWeight(), 1);
    ASSERT_TRUE(base.GetMask());
    EXPECT_EQ(base.GetMask()->GetWeights(), (std::vector<float>{ 1, 1, 1, 1 }));
    EXPECT_FALSE(base.SetWeight(0.5f));
    EXPECT_FALSE(instance.SetLayerWeight("Base", 0));
    EXPECT_TRUE(instance.SetLayerWeight("Base", 1));
    EXPECT_FALSE(instance.RemoveLayer("Base"));
    EXPECT_FALSE(instance.AddLayer("Base", base.GetMask()));
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton()))));
    EXPECT_EQ(&instance.GetBaseLayer(), &base);
    ASSERT_TRUE(instance.Seek(0.5f));
    EXPECT_EQ(&instance.GetBaseLayer(), &base);
    auto mask = BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine");
    for (int i = 0; i < 16; ++i)
        ASSERT_TRUE(instance.AddLayer("Upper" + std::to_string(i), mask));
    EXPECT_EQ(instance.GetLayerCount(), 17);
    EXPECT_EQ(instance.GetLayer("Base"), &base);
    instance.ClearLayers();
    EXPECT_EQ(instance.GetLayerCount(), 1);
    EXPECT_EQ(&instance.GetBaseLayer(), &base);
    auto pose = base.GetPlayer().GetLocalPose();
    ASSERT_TRUE(instance.SetLocalPose(pose));
    EXPECT_EQ(&instance.GetBaseLayer(), &base);
    instance.Stop();
    EXPECT_EQ(instance.GetLayerCount(), 1);
    EXPECT_EQ(instance.GetLayer("Base"), &base);
    EXPECT_FALSE(base.GetPlayer().HasPose());
    const auto& readOnly = instance;
    EXPECT_EQ(&readOnly.GetBaseLayer(), &base);
}

TEST(AnimationLayerTest, BaseAndAdditionalLayersExposeTheSamePlaybackAndSettings)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& base = instance.GetBaseLayer();
    auto walk = MakeLayerBase(mesh->GetSkeleton())->WithNotifies({{ "Step", 0.25f }});
    auto space = BlendSpace1D::Create("Locomotion", {{ 0, walk }});
    ASSERT_TRUE(base.GetPlayer().Play(MakeNode(space), { 0 }));
    auto mask = BoneMask::FromBone("UpperMask", mesh->GetSkeleton(), "Spine");
    ASSERT_TRUE(instance.AddLayer("Upper", mask));
    auto* upper = instance.GetLayer("Upper");
    ASSERT_NE(upper, nullptr);
    EXPECT_EQ(upper->GetName(), "Upper");
    EXPECT_EQ(upper->GetMask(), mask);
    ASSERT_TRUE(upper->SetWeight(0.5f));
    const auto action = MakeLayerAction(mesh->GetSkeleton())->WithNotifies({{ "Hit", 0.25f }});
    ASSERT_TRUE(upper->GetPlayer().Play(MakeNode(action)));
    ASSERT_TRUE(instance.Update(0.25f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 6.875f, 0 });
    ExpectPosition(instance.GetLocalPose().LocalTransforms[3].Translation, { 0, -1.25f, 0 });
    EXPECT_FLOAT_EQ(instance.GetTime(), base.GetPlayer().GetTime());
    ASSERT_EQ(base.GetPlayer().GetNotifyEvents().size(), 1);
    EXPECT_EQ(base.GetPlayer().GetNotifyEvents().front().LayerName, "Base");
    ASSERT_EQ(upper->GetPlayer().GetNotifyEvents().size(), 1);
    EXPECT_EQ(upper->GetPlayer().GetNotifyEvents().front().LayerName, "Upper");
    ASSERT_TRUE(instance.SetLayerWeight("Upper", 0));
    EXPECT_FLOAT_EQ(upper->GetWeight(), 0);
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1.25f, 0 });
}

TEST(AnimationLayerTest, InstanceResumePreservesIndependentBasePause)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& base = instance.GetBaseLayer();
    ASSERT_TRUE(base.GetPlayer().Play(MakeNode(MakeLayerBase(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* upper = instance.GetLayer("Upper");
    ASSERT_TRUE(upper->GetPlayer().Play(MakeNode(MakeLayerAction(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.Update(0.25f));
    base.GetPlayer().Pause();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(instance.IsPlaying());
    EXPECT_FLOAT_EQ(base.GetPlayer().GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(upper->GetPlayer().GetTime(), 0.5f);
    instance.Pause();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(instance.IsPlaying());
    instance.Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(base.GetPlayer().IsPlaying());
    EXPECT_FLOAT_EQ(base.GetPlayer().GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(upper->GetPlayer().GetTime(), 0.75f);
    base.GetPlayer().Resume();
    ASSERT_TRUE(instance.Update(0.125f));
    EXPECT_FLOAT_EQ(base.GetPlayer().GetTime(), 0.375f);
}

TEST(AnimationLayerTest, FailedManualPoseDoesNotChangeTheBaseOrRemoveLayers)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& base = instance.GetBaseLayer();
    const auto locomotion = MakeLayerBase(mesh->GetSkeleton())->WithNotifies({{ "Step", 0 }});
    ASSERT_TRUE(base.GetPlayer().Play(MakeNode(locomotion)));
    ASSERT_TRUE(instance.AddLayer("Upper", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    ASSERT_TRUE(instance.GetLayer("Upper")->GetPlayer().Play(
        MakeNode(MakeLayerAction(mesh->GetSkeleton()))
    ));
    ASSERT_TRUE(instance.Update(0.125f));
    auto pose = base.GetPlayer().GetLocalPose();
    pose.LocalTransforms[0].Translation.x = std::numeric_limits<float>::max();
    pose.LocalTransforms[1].Translation.x = std::numeric_limits<float>::max();
    EXPECT_FALSE(instance.SetLocalPose(pose));
    EXPECT_EQ(instance.GetLayerCount(), 2);
    EXPECT_EQ(&instance.GetBaseLayer(), &base);
    EXPECT_FLOAT_EQ(base.GetPlayer().GetTime(), 0.125f);
    EXPECT_TRUE(base.GetPlayer().IsPlaying());
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().LayerName, "Base");
    ASSERT_TRUE(instance.Update(0.125f));
    EXPECT_FLOAT_EQ(base.GetPlayer().GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetLayer("Upper")->GetPlayer().GetTime(), 0.25f);
}

TEST(AdditiveAnimationLayerTest, DefaultsToOverrideAndKeepsTheBaseModeFixed)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& base = instance.GetBaseLayer();
    EXPECT_EQ(base.GetBlendMode(), EAnimationLayerBlendMode::Override);
    EXPECT_FALSE(base.SetBlendMode(EAnimationLayerBlendMode::Additive));
    EXPECT_TRUE(base.SetBlendMode(EAnimationLayerBlendMode::Override));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    EXPECT_EQ(recoil->GetBlendMode(), EAnimationLayerBlendMode::Override);
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    EXPECT_EQ(recoil->GetBlendMode(), EAnimationLayerBlendMode::Additive);
}

TEST(AdditiveAnimationLayerTest, ReferencePoseAndZeroWeightPreserveUnderlyingTransforms)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto original = instance.GetLocalPose();
    original.LocalTransforms[1].Translation = { 3, 4, 5 };
    original.LocalTransforms[1].Rotation = glm::angleAxis(0.7f, glm::vec3(0, 0, 1));
    original.LocalTransforms[1].Scale = { 2, 3, 4 };
    ASSERT_TRUE(instance.SetLocalPose(original));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    auto sample = recoil->GetAdditiveReferencePose();
    sample.LocalTransforms[1].Rotation = -sample.LocalTransforms[1].Rotation;
    ASSERT_TRUE(recoil->GetPlayer().SetLocalPose(sample));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, original.LocalTransforms[1].Translation);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Scale, original.LocalTransforms[1].Scale);
    EXPECT_NEAR(std::abs(glm::dot(instance.GetLocalPose().LocalTransforms[1].Rotation, original.LocalTransforms[1].Rotation)), 1, 1e-5f);
    sample.LocalTransforms[1].Translation = { 50, 50, 50 };
    ASSERT_TRUE(recoil->GetPlayer().SetLocalPose(sample));
    ASSERT_TRUE(recoil->SetWeight(0));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, original.LocalTransforms[1].Translation);
}

TEST(AdditiveAnimationLayerTest, WeightsTranslationParentFrameRotationAndScaleRatios)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto original = instance.GetLocalPose();
    original.LocalTransforms[1].Translation = { 10, 20, 30 };
    original.LocalTransforms[1].Rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
    original.LocalTransforms[1].Scale = { 3, 6, 2 };
    ASSERT_TRUE(instance.SetLocalPose(original));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine"), 0.5f));
    auto* recoil = instance.GetLayer("Recoil");
    auto reference = recoil->GetAdditiveReferencePose();
    reference.LocalTransforms[1].Translation = { 1, 2, 3 };
    reference.LocalTransforms[1].Rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 1, 0));
    reference.LocalTransforms[1].Scale = { 2, 4, 0.5f };
    auto sample = reference;
    sample.LocalTransforms[1].Translation = { 3, 1, 7 };
    sample.LocalTransforms[1].Rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0)) * reference.LocalTransforms[1].Rotation;
    sample.LocalTransforms[1].Scale = { 4, 2, 1.5f };
    ASSERT_TRUE(recoil->SetAdditiveReferencePose(reference));
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().SetLocalPose(sample));
    ASSERT_TRUE(instance.Update(0));
    const auto& actual = instance.GetLocalPose().LocalTransforms[1];
    ExpectPosition(actual.Translation, { 11, 19.5f, 32 });
    ExpectPosition(actual.Scale, { 4.5f, 4.5f, 4 });
    ExpectPosition(actual.Rotation * glm::vec3(1, 0, 0),
        { 0, std::sqrt(0.5f), std::sqrt(0.5f) });
}

TEST(AdditiveAnimationLayerTest, FullWeightReconstructsSampleFromItsReference)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto reference = instance.GetLocalPose();
    reference.LocalTransforms[1].Rotation = glm::angleAxis(0.6f, glm::normalize(glm::vec3(1, 2, 3)));
    reference.LocalTransforms[1].Scale = { 2, 3, 4 };
    ASSERT_TRUE(instance.SetLocalPose(reference));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    ASSERT_TRUE(recoil->SetAdditiveReferencePose(reference));
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    auto sample = reference;
    sample.LocalTransforms[1].Translation = { 2, 3, 4 };
    sample.LocalTransforms[1].Rotation = glm::angleAxis(-0.9f, glm::normalize(glm::vec3(3, 2, 1)));
    sample.LocalTransforms[1].Scale = { 1, 6, 2 };
    ASSERT_TRUE(recoil->GetPlayer().SetLocalPose(sample));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, sample.LocalTransforms[1].Translation);
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Scale, sample.LocalTransforms[1].Scale);
    EXPECT_NEAR(std::abs(glm::dot(instance.GetLocalPose().LocalTransforms[1].Rotation, sample.LocalTransforms[1].Rotation)), 1, 1e-5f);
}

TEST(AdditiveAnimationLayerTest, MultipliesBoneAndLayerWeightsWithoutMovingLegs)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.AddLayer("Recoil",
        BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine", 0.5f), 0.5f));
    auto* recoil = instance.GetLayer("Recoil");
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(MakeLayerAction(mesh->GetSkeleton()))));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 5, 0 });
    ExpectPosition(instance.GetLocalPose().LocalTransforms[2].Translation, { 0.5f, 1, 0 });
    ExpectPosition(instance.GetLocalPose().LocalTransforms[3].Translation, { 0, -1.5f, 0 });
}

TEST(AdditiveAnimationLayerTest, ComposesWithOverrideLayersInInsertionOrder)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto mask = BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine");
    ASSERT_TRUE(instance.AddLayer("Action", mask));
    ASSERT_TRUE(instance.GetLayer("Action")->GetPlayer().Play(
        MakeNode(MakeLayerAction(mesh->GetSkeleton()))
    ));
    ASSERT_TRUE(instance.AddLayer("Recoil", mask, 0.5f));
    auto* recoil = instance.GetLayer("Recoil");
    auto sample = recoil->GetAdditiveReferencePose();
    sample.LocalTransforms[1].Translation.y = 2;
    ASSERT_TRUE(recoil->GetPlayer().SetLocalPose(sample));
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 10.5f);
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Override));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 6);
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(instance.AddLayer("Last", mask));
    auto lastPose = sample;
    lastPose.LocalTransforms[1].Translation.y = 30;
    ASSERT_TRUE(instance.GetLayer("Last")->GetPlayer().SetLocalPose(lastPose));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 30);
}

TEST(AdditiveAnimationLayerTest, UsesASampledReferenceAndCanResetItWithoutSeeking)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    auto action = MakeLayerAction(mesh->GetSkeleton());
    SLocalPose reference;
    ASSERT_TRUE(action->Sample(0.25f, reference, mesh->GetData().ReferencePose));
    ASSERT_TRUE(recoil->SetAdditiveReferencePose(reference));
    ASSERT_TRUE(recoil->SetAdditiveReferencePose(recoil->GetAdditiveReferencePose()));
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(action)));
    ASSERT_TRUE(recoil->GetPlayer().Seek(0.25f));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 1, 0 });
    ASSERT_TRUE(recoil->GetPlayer().Seek(0.5f));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 3.5f, 0 });
    ASSERT_TRUE(recoil->SetAdditiveReferencePose({}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(recoil->GetPlayer().GetTime(), 0.5f);
    EXPECT_EQ(recoil->GetAdditiveReferencePose().LocalTransforms.size(), mesh->GetData().ReferencePose.LocalTransforms.size());
    ExpectPosition(instance.GetLocalPose().LocalTransforms[1].Translation, { 0, 15, 0 });
}

TEST(AdditiveAnimationLayerTest, RejectsInvalidReferenceWithoutChangingThePreviousValue)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    auto reference = recoil->GetAdditiveReferencePose();
    reference.LocalTransforms[1].Translation = { 4, 5, 6 };
    ASSERT_TRUE(recoil->SetAdditiveReferencePose(reference));
    auto invalid = reference;
    invalid.LocalTransforms.pop_back();
    EXPECT_FALSE(recoil->SetAdditiveReferencePose(invalid));
    invalid = reference;
    invalid.LocalTransforms[1].Scale.x = 0;
    EXPECT_FALSE(recoil->SetAdditiveReferencePose(invalid));
    invalid = reference;
    invalid.LocalTransforms[1].Rotation = { 0, 0, 0, 0 };
    EXPECT_FALSE(recoil->SetAdditiveReferencePose(invalid));
    invalid = reference;
    invalid.LocalTransforms[1].Translation.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(recoil->SetAdditiveReferencePose(invalid));
    ExpectPosition(recoil->GetAdditiveReferencePose().LocalTransforms[1].Translation, { 4, 5, 6 });
}

TEST(AdditiveAnimationLayerTest, BlendsSourcesAndInterruptedFadesBeforeApplyingDifferences)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto original = instance.GetLocalPose();
    original.LocalTransforms[1].Translation.y = 10;
    ASSERT_TRUE(instance.SetLocalPose(original));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    auto neutral = Sequence::Create("Neutral", mesh->GetSkeleton(), 1, {});
    auto pulse = Sequence::Create("Pulse", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 3, 0 } }} },
    });
    auto space = BlendSpace1D::Create("Recoil", {{ 0, neutral }, { 1, pulse }});
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(space), { 0.5f }));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 11);
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(pulse), 0.5f));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 11.5f);
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(neutral), 0.5f));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 11.5f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 10.75f);
}

TEST(AdditiveAnimationLayerTest, SingularScaleRollsBackAllClocksAndEvents)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(MakeLayerBase(mesh->GetSkeleton())->WithNotifies({{ "Step", 0 }}))));
    ASSERT_TRUE(instance.Update(0.125f));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine"), 0.5f));
    auto* recoil = instance.GetLayer("Recoil");
    auto inverted = Sequence::Create("Inverted", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(inverted)));
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.125f);
    EXPECT_FLOAT_EQ(recoil->GetPlayer().GetTime(), 0);
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().GetName(), "Step");
    ASSERT_TRUE(recoil->SetWeight(1));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.375f);
    EXPECT_FLOAT_EQ(recoil->GetPlayer().GetTime(), 0.25f);
}

TEST(AdditiveAnimationLayerTest, ScaleRatioOverflowFailsSafelyAndCanBeMuted)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    auto reference = recoil->GetAdditiveReferencePose();
    reference.LocalTransforms[1].Scale.x = 1e-8f;
    ASSERT_TRUE(recoil->SetAdditiveReferencePose(reference));
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    auto extreme = Sequence::Create("Extreme", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Scales = {{ 0, { std::numeric_limits<float>::max(), 1, 1 } }} },
    });
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(extreme)));
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(recoil->GetPlayer().GetTime(), 0);
    ASSERT_TRUE(recoil->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(recoil->GetPlayer().GetTime(), 0.25f);
}

TEST(AdditiveAnimationLayerTest, KeepsNotifySourcesAndSuppressesMutedEvents)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    auto clip = MakeLayerAction(mesh->GetSkeleton())->WithNotifies({{ "Shot", 0.25f, 0, 42 }});
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(clip)));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().Sequence, clip);
    EXPECT_EQ(instance.GetNotifyEvents().front().LayerName, "Recoil");
    ASSERT_NE(instance.GetNotifyEvents().front().GetPayload<int>(), nullptr);
    EXPECT_EQ(*instance.GetNotifyEvents().front().GetPayload<int>(), 42);
    ASSERT_TRUE(recoil->SetWeight(0));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(recoil->SetWeight(1));
    ASSERT_TRUE(instance.Update(0.75f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents().front().LayerName, "Recoil");
}

TEST(AdditiveAnimationLayerTest, CompletedPulseReturnsToTheUnderlyingPose)
{
    auto mesh = SkeletalMesh::Create(MakeData(MakeLayerSkeleton()));
    const auto instanceHandle = AnimationTestCharacter::Create(mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto original = instance.GetLocalPose();
    original.LocalTransforms[1].Translation.y = 10;
    ASSERT_TRUE(instance.SetLocalPose(original));
    ASSERT_TRUE(instance.AddLayer("Recoil", BoneMask::FromBone("Upper", mesh->GetSkeleton(), "Spine")));
    auto* recoil = instance.GetLayer("Recoil");
    auto pulse = Sequence::Create("Pulse", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Translations = {
            { 0, { 0, 1, 0 } }, { 0.25f, { 0, 3, 0 } }, { 1, { 0, 1, 0 } },
        } },
    });
    ASSERT_TRUE(recoil->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(recoil->GetPlayer().Play(MakeNode(pulse), false));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 12);
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_FALSE(recoil->GetPlayer().IsPlaying());
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 10);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[1].Translation.y, 10);
}
