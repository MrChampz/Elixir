#include <gtest/gtest.h>

#include <Engine/Animation/AnimationSequence.h>
#include <Engine/Mesh/SkeletalMeshInstance.h>
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

    Ref<AnimationSequence> MakeSequence(const Ref<const Skeleton>& skeleton)
    {
        return AnimationSequence::Create("RaiseHand", skeleton, 2, {
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
    std::vector<SBoneTransform> local;
    for (const auto& bone : skeleton->GetBones()) local.push_back(bone.ReferenceTransform);
    std::vector<glm::mat4> model;
    ASSERT_TRUE(skeleton->BuildModelPose(local, model));
    ExpectPosition(glm::vec3(model[1][3]), { 2, 1, 0 });
    local.pop_back();
    EXPECT_FALSE(skeleton->BuildModelPose(local, model));
    ASSERT_EQ(model.size(), 2);
    ExpectPosition(glm::vec3(model[1][3]), { 2, 1, 0 });
}

TEST(SkeletonTest, CompatibilityUsesNamesAndHierarchy)
{
    EXPECT_TRUE(MakeSkeleton()->IsCompatible(*MakeSkeleton()));
    auto other = Skeleton::Create("Other", {{ .Name = "Root" }, { .Name = "Hand" }});
    EXPECT_FALSE(MakeSkeleton()->IsCompatible(*other));
}

TEST(AnimationSequenceTest, SamplesChannelsAndRetainsMissingReferenceChannels)
{
    auto skeleton = MakeSkeleton();
    auto sequence = MakeSequence(skeleton);
    ASSERT_TRUE(sequence);
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(sequence->Sample(1, pose));
    ExpectPosition(pose[0].Translation, { 2, 0, 0 });
    ExpectPosition(pose[1].Translation, { 0, 2, 0 });
    ASSERT_TRUE(sequence->Sample(-3, pose));
    ExpectPosition(pose[1].Translation, { 0, 1, 0 });
    ASSERT_TRUE(sequence->Sample(9, pose));
    ExpectPosition(pose[1].Translation, { 0, 3, 0 });
    EXPECT_FALSE(sequence->Sample(std::numeric_limits<float>::quiet_NaN(), pose));
    ExpectPosition(pose[1].Translation, { 0, 3, 0 });
}

TEST(AnimationSequenceTest, SupportsStepAndShortestPathQuaternionInterpolation)
{
    SBoneAnimationTrack track{ .BoneIndex = 1,
        .Translations = {{ 0, { 0, 1, 0 } }, { 2, { 0, 3, 0 } }},
        .Rotations = {{ 0, { 1, 0, 0, 0 } }, { 2, { -1, 0, 0, 0 } }},
        .TranslationInterpolation = EBoneInterpolation::Step };
    auto sequence = AnimationSequence::Create("Step", MakeSkeleton(), 2, { track });
    ASSERT_TRUE(sequence);
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(sequence->Sample(1, pose));
    ExpectPosition(pose[1].Translation, { 0, 1, 0 });
    EXPECT_NEAR(std::abs(pose[1].Rotation.w), 1, 1e-5f);
    ASSERT_TRUE(sequence->Sample(2, pose));
    ExpectPosition(pose[1].Translation, { 0, 3, 0 });
}

TEST(AnimationSequenceTest, RejectsInvalidKeysAndDuplicateTracks)
{
    auto skeleton = MakeSkeleton();
    EXPECT_FALSE(AnimationSequence::Create("Bad", skeleton, -1, {}));
    EXPECT_FALSE(AnimationSequence::Create("Bad", nullptr, 1, {}));
    EXPECT_FALSE(AnimationSequence::Create("Bad", skeleton, 1, {{ .BoneIndex = 5 }}));
    EXPECT_FALSE(AnimationSequence::Create("Bad", skeleton, 1, {{ .BoneIndex = 0 }, { .BoneIndex = 0 }}));
    EXPECT_FALSE(AnimationSequence::Create("Bad", skeleton, 1,
        {{ .Translations = {{ 0, {} }, { 0, {} }} }}));
    EXPECT_FALSE(AnimationSequence::Create("Bad", skeleton, 1,
        {{ .Translations = {{ 2, {} }} }}));
    EXPECT_FALSE(AnimationSequence::Create("Bad", skeleton, 1,
        {{ .Rotations = {{ 0, { 0, 0, 0, 0 } }} }}));
}

TEST(SkeletalMeshTest, ReferencePoseAndReorderedPalettePreserveGeometry)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    SkeletalMeshInstance meshInstance(mesh);
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Position, { 2, 1, 0 });
    ExpectPosition(vertices[1].Position, { 3, 1, 0 });
    ASSERT_TRUE(meshInstance.Play(MakeSequence(mesh->GetSkeleton())));
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
    SkeletalMeshInstance meshInstance(mesh);
    ASSERT_TRUE(meshInstance.Play(MakeSequence(mesh->GetSkeleton())));
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
    SkeletalMeshInstance meshInstance(mesh);
    auto pose = meshInstance.GetLocalPose();
    pose[0].Scale = { 2, 1, 1 };
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
    SkeletalMeshInstance meshInstance(mesh);
    std::vector<SStaticMeshVertex> vertices(1);
    vertices[0].Position = { 9, 8, 7 };
    EXPECT_FALSE(mesh->SkinVertices({}, vertices));
    auto model = meshInstance.GetModelPose(); model[0] = glm::mat4(0);
    EXPECT_FALSE(mesh->SkinVertices(model, vertices));
    ASSERT_EQ(vertices.size(), 1);
    ExpectPosition(vertices[0].Position, { 9, 8, 7 });
}

TEST(SkeletalMeshInstanceTest, InstancesShareAssetsWithIndependentPlayback)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    auto sequence = MakeSequence(mesh->GetSkeleton());
    SkeletalMeshInstance first(mesh), second(mesh);
    ASSERT_TRUE(first.Play(sequence));
    ASSERT_TRUE(second.Play(sequence));
    ASSERT_TRUE(first.Update(1));
    EXPECT_FLOAT_EQ(first.GetTime(), 1);
    EXPECT_FLOAT_EQ(second.GetTime(), 0);
    ExpectPosition(first.GetLocalPose()[1].Translation, { 0, 2, 0 });
    ExpectPosition(second.GetLocalPose()[1].Translation, { 0, 1, 0 });
}

TEST(SkeletalMeshInstanceTest, HandlesLoopingPauseSeekAndCompletion)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance meshInstance(mesh);
    auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(meshInstance.Play(sequence));
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
    ASSERT_TRUE(meshInstance.Play(sequence, false));
    ASSERT_TRUE(meshInstance.Update(3));
    EXPECT_FALSE(meshInstance.IsPlaying());
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 2);
    ExpectPosition(meshInstance.GetLocalPose()[1].Translation, { 0, 3, 0 });
    meshInstance.Stop();
    ExpectPosition(meshInstance.GetLocalPose()[1].Translation, { 0, 1, 0 });
    EXPECT_FALSE(meshInstance.Seek(0));
}

TEST(SkeletalMeshInstanceTest, FailedOperationsPreservePlaybackAndPose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance meshInstance(mesh);
    ASSERT_TRUE(meshInstance.Play(MakeSequence(mesh->GetSkeleton())));
    ASSERT_TRUE(meshInstance.Update(1));
    EXPECT_FALSE(meshInstance.Play(AnimationSequence::Create("Wrong", Skeleton::Create("Other", {{ .Name = "Other" }}), 1, {})));
    EXPECT_FALSE(meshInstance.Update(-1));
    EXPECT_FALSE(meshInstance.Update(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(meshInstance.SetLocalPose({}));
    EXPECT_FLOAT_EQ(meshInstance.GetTime(), 1);
    EXPECT_TRUE(meshInstance.IsPlaying());
    EXPECT_EQ(meshInstance.GetMesh(), mesh);
}

TEST(SkeletalMeshInstanceTest, MeshSpecificReferencePoseSuppliesUnanimatedBones)
{
    auto data = MakeData();
    for (const auto& bone : data.Skeleton->GetBones()) data.ReferencePose.push_back(bone.ReferenceTransform);
    data.ReferencePose[0].Translation = { 10, 0, 0 };
    auto mesh = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(mesh);
    SkeletalMeshInstance meshInstance(mesh);
    ASSERT_TRUE(meshInstance.Play(MakeSequence(mesh->GetSkeleton())));
    ASSERT_TRUE(meshInstance.Update(1));
    ExpectPosition(meshInstance.GetLocalPose()[0].Translation, { 10, 0, 0 });
    meshInstance.Stop();
    ExpectPosition(meshInstance.GetLocalPose()[0].Translation, { 10, 0, 0 });
}

TEST(SkeletalMeshInstanceTest, ZeroDurationSequenceProducesAStablePose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance meshInstance(mesh);
    auto sequence = AnimationSequence::Create("Still", mesh->GetSkeleton(), 0,
        {{ .BoneIndex = 1, .Translations = {{ 0, { 0, 4, 0 } }} }});
    ASSERT_TRUE(meshInstance.Play(sequence));
    EXPECT_FALSE(meshInstance.IsPlaying());
    meshInstance.Resume();
    EXPECT_FALSE(meshInstance.IsPlaying());
    ASSERT_TRUE(meshInstance.Update(100));
    ExpectPosition(meshInstance.GetLocalPose()[1].Translation, { 0, 4, 0 });
}

TEST(SkeletalAnimationApiTest, SharedAssetsCannotBeReassigned)
{
    static_assert(!std::is_copy_constructible_v<Skeleton>);
    static_assert(!std::is_copy_assignable_v<Skeleton>);
    static_assert(!std::is_copy_assignable_v<SkeletalMesh>);
    static_assert(!std::is_copy_assignable_v<AnimationSequence>);
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
    SkeletalMeshInstance meshInstance(mesh);
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), vertices));
    ASSERT_EQ(vertices.size(), 3);
    EXPECT_EQ(vertices[0].TexCoord, glm::vec2(0.25f, 0.75f));
    EXPECT_EQ(vertices[0].TexCoord1, glm::vec2(0.5f, 0.125f));
    EXPECT_EQ(vertices[0].Tangent, glm::vec4(1, 0, 0, -1));
    EXPECT_EQ(mesh->GetData().Vertices[0].Position, glm::vec3(2, 1, 0));
}

TEST(SkeletalMeshInstanceTest, CrossFadeAdvancesBothSequencesAndPreservesTheInitialPose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto target = AnimationSequence::Create("LowerHand", mesh->GetSkeleton(), 4, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 9, 0 } }, { 4, { 0, 13, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeSequence(mesh->GetSkeleton())));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_TRUE(instance.Play(target, 2.0f));
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.0f);
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 1.5f, 0 });
    ASSERT_TRUE(instance.Update(0.5f));
    // Source at 1 second: y=2. Target at 0.5 seconds: y=9.5. Weight: 0.25.
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 3.875f, 0 });
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 2.0f);
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 11, 0 });
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 11.5f, 0 });
}

TEST(SkeletalMeshInstanceTest, CrossFadeBlendsLocalTranslationScaleAndRotationBeforeHierarchy)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    const auto rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0, 0, 1));
    auto target = AnimationSequence::Create("Turn", mesh->GetSkeleton(), 2, {
        { .BoneIndex = 0, .Translations = {{ 0, { 6, 0, 0 } }},
          .Rotations = {{ 0, -rotation }}, .Scales = {{ 0, { 3, 3, 3 } }} },
    });
    ASSERT_TRUE(instance.Play(target, 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose()[0].Translation, { 4, 0, 0 });
    ExpectPosition(instance.GetLocalPose()[0].Scale, { 2, 2, 2 });
    const auto expectedRotation = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1));
    EXPECT_NEAR(std::abs(glm::dot(instance.GetLocalPose()[0].Rotation, expectedRotation)),
        1.0f, 1e-5f);
    // The unanimated child retains its local reference pose and inherits the blended root.
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 1, 0 });
    ExpectPosition(glm::vec3(instance.GetModelPose()[1][3]), { 4 - std::sqrt(3.0f), 1, 0 });
    std::vector<SStaticMeshVertex> vertices;
    ASSERT_TRUE(mesh->SkinVertices(instance.GetModelPose(), vertices));
    ExpectPosition(vertices[0].Position, { 4 - std::sqrt(3.0f), 1, 0 });
}

TEST(SkeletalMeshInstanceTest, InterruptedCrossFadeStartsFromTheVisiblePose)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto first = AnimationSequence::Create("First", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 5, 0 } }} },
    });
    auto second = AnimationSequence::Create("Second", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 9, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(first, 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Play(second, 2.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Update(0.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 6, 0 });
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 9, 0 });
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FALSE(instance.IsPlaying());
}

TEST(SkeletalMeshInstanceTest, PauseAndResumeControlTransitionTime)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto target = AnimationSequence::Create("Still", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 5, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(target, 2.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    instance.Pause();
    ASSERT_TRUE(instance.Update(10.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 2, 0 });
    EXPECT_TRUE(instance.IsTransitioning());
    instance.Resume();
    EXPECT_TRUE(instance.IsPlaying());
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 3, 0 });
    ASSERT_TRUE(instance.Update(10.0f));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 5, 0 });
}

TEST(SkeletalMeshInstanceTest, NonloopingTargetFinishesOnlyAfterTheFade)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    ASSERT_TRUE(instance.Play(MakeSequence(mesh->GetSkeleton()), 4.0f, false));
    ASSERT_TRUE(instance.Update(2.0f));
    EXPECT_TRUE(instance.IsPlaying());
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 2.0f);
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 2, 0 });
    ASSERT_TRUE(instance.Update(2.0f));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 3, 0 });
}

TEST(SkeletalMeshInstanceTest, CrossFadeWrapsSourceAndTargetIndependently)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto target = AnimationSequence::Create("Target", mesh->GetSkeleton(), 1, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 5, 0 } }, { 1, { 0, 9, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeSequence(mesh->GetSkeleton())));
    ASSERT_TRUE(instance.Update(1.5f));
    ASSERT_TRUE(instance.Play(target, 3.0f));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.5f);
    // Source wraps to t=1 (y=2); target wraps to t=0.5 (y=7).
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 4.5f, 0 });
}

TEST(SkeletalMeshInstanceTest, CrossFadeClampsNonloopingSourceAndSupportsImmediateSwitch)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto target = AnimationSequence::Create("Target", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 9, 0 } }} },
    });
    ASSERT_TRUE(instance.Play(MakeSequence(mesh->GetSkeleton()), false));
    ASSERT_TRUE(instance.Update(1.5f));
    ASSERT_TRUE(instance.Play(target, 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 6, 0 });
    ASSERT_TRUE(instance.Play(target, 0.0f));
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FALSE(instance.IsPlaying());
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 9, 0 });
}

TEST(SkeletalMeshInstanceTest, SeekStopAndManualPoseCancelTransitions)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(instance.Play(sequence, 2.0f));
    ASSERT_TRUE(instance.Seek(1.0f));
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 2, 0 });
    ASSERT_TRUE(instance.Play(sequence, 2.0f));
    instance.Stop();
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 1, 0 });
    auto pose = instance.GetLocalPose();
    pose[1].Translation = { 0, 7, 0 };
    ASSERT_TRUE(instance.Play(sequence, 2.0f));
    ASSERT_TRUE(instance.SetLocalPose(pose));
    EXPECT_FALSE(instance.IsTransitioning());
    EXPECT_FALSE(instance.IsPlaying());
    ASSERT_TRUE(instance.Play(sequence, 2.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 4.5f, 0 });
    SkeletalMeshInstance replacement(mesh);
    EXPECT_FALSE(replacement.IsTransitioning());
    ExpectPosition(replacement.GetLocalPose()[1].Translation, { 0, 1, 0 });
    EXPECT_TRUE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 4.5f, 0 });
}

TEST(SkeletalMeshInstanceTest, InvalidFadeRequestsPreserveAnActiveTransition)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    const auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(instance.Play(sequence, 2.0f));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto before = instance.GetLocalPose()[1].Translation;
    EXPECT_FALSE(instance.Play(sequence, -1.0f));
    EXPECT_FALSE(instance.Play(sequence, std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(instance.Play(sequence, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(instance.Play(nullptr, 1.0f));
    auto incompatible = AnimationSequence::Create("Other",
        Skeleton::Create("Other", {{ .Name = "Other" }}), 0, {});
    EXPECT_FALSE(instance.Play(incompatible, 1.0f));
    EXPECT_FALSE(instance.Update(-1.0f));
    EXPECT_FALSE(instance.Seek(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.5f);
    ExpectPosition(instance.GetLocalPose()[1].Translation, before);
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 1.5f, 0 });
}

TEST(SkeletalMeshInstanceTest, SingularBlendedScalePreservesPoseAndTransitionProgress)
{
    auto mesh = SkeletalMesh::Create(MakeData());
    SkeletalMeshInstance instance(mesh);
    auto target = AnimationSequence::Create("Mirror", mesh->GetSkeleton(), 0, {
        { .BoneIndex = 0, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    ASSERT_TRUE(instance.Play(target, 2.0f));
    EXPECT_FALSE(instance.Update(1.0f));
    EXPECT_TRUE(instance.IsTransitioning());
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.0f);
    ExpectPosition(instance.GetLocalPose()[0].Scale, { 1, 1, 1 });
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(instance.GetLocalPose()[0].Scale, { 0.5f, 1, 1 });
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FALSE(instance.IsTransitioning());
    ExpectPosition(instance.GetLocalPose()[0].Scale, { -1, 1, 1 });
}

TEST(SkeletalMeshInstanceTest, CrossFadeUsesMeshSpecificDefaultsAndIndependentInstanceState)
{
    auto data = MakeData();
    for (const auto& bone : data.Skeleton->GetBones())
        data.ReferencePose.push_back(bone.ReferenceTransform);
    data.ReferencePose[0].Translation = { 10, 0, 0 };
    auto mesh = SkeletalMesh::Create(std::move(data));
    SkeletalMeshInstance first(mesh), second(mesh);
    auto sequence = MakeSequence(mesh->GetSkeleton());
    ASSERT_TRUE(first.Play(sequence, 2.0f));
    ASSERT_TRUE(second.Play(sequence, 4.0f));
    ASSERT_TRUE(first.Update(1.0f));
    ASSERT_TRUE(second.Update(1.0f));
    ExpectPosition(first.GetLocalPose()[1].Translation, { 0, 1.5f, 0 });
    ExpectPosition(second.GetLocalPose()[1].Translation, { 0, 1.25f, 0 });
    ExpectPosition(first.GetLocalPose()[0].Translation, { 10, 0, 0 });
    ExpectPosition(second.GetLocalPose()[0].Translation, { 10, 0, 0 });
}

TEST(SkeletalMeshInstanceTest, MeshOwnershipIsFixedAtConstruction)
{
    static_assert(!std::is_default_constructible_v<SkeletalMeshInstance>);
    static_assert(!std::is_copy_assignable_v<SkeletalMeshInstance>);
    static_assert(!std::is_move_assignable_v<SkeletalMeshInstance>);
    static_assert(std::is_same_v<decltype(std::declval<SkeletalMeshInstance&>().GetMesh()),
        const Ref<const SkeletalMesh>&>);

    auto mesh = SkeletalMesh::Create(MakeData());
    ASSERT_TRUE(mesh);
    const std::weak_ptr<const SkeletalMesh> weak = mesh;
    {
        SkeletalMeshInstance instance(mesh);
        mesh.reset();
        EXPECT_FALSE(weak.expired());
        ASSERT_TRUE(instance.Play(MakeSequence(instance.GetMesh()->GetSkeleton())));
        ASSERT_TRUE(instance.Update(1));
        instance.Stop();
        EXPECT_EQ(instance.GetMesh(), weak.lock());
        ExpectPosition(instance.GetLocalPose()[1].Translation, { 0, 1, 0 });
    }
    EXPECT_TRUE(weak.expired());
}

TEST(SkeletalMeshInstanceTest, NullMeshConstructionLeavesAnInertInstance)
{
    SkeletalMeshInstance instance(nullptr);
    EXPECT_FALSE(instance.GetMesh());
    EXPECT_TRUE(instance.GetLocalPose().empty());
    EXPECT_TRUE(instance.GetModelPose().empty());
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FALSE(instance.Play(nullptr));
    EXPECT_FALSE(instance.SetLocalPose({}));
    EXPECT_FALSE(instance.Seek(0));
    instance.Stop();
    EXPECT_TRUE(instance.Update(0));
}
