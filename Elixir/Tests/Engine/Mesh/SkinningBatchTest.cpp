#include <Engine/Animation/Animation.h>
#include <gtest/gtest.h>
#include <Engine/Mesh/SkinningBatch.h>
#include "SkinningTestData.h"

using namespace Elixir;

TEST(SkinningBatchTest, LayoutMatchesShaderByteOffsets)
{
    static_assert(sizeof(SStaticMeshVertex) == 56);
    static_assert(sizeof(SSkeletalMeshVertex) == 88);
    static_assert(offsetof(SSkeletalMeshVertex, Joints) == 56);
    static_assert(offsetof(SSkeletalMeshVertex, Weights) == 72);
    static_assert(offsetof(SStaticMeshVertex, TexCoord1) == 48);
    static_assert(sizeof(glm::mat4) == 64);
    static_assert(sizeof(SSkinningJob) == 16);
    static_assert(sizeof(SSkinningBlock) == 8);
    SUCCEED();
}

TEST(SkinningBatchTest, BatchesIndependentPosesAndPartialWorkgroups)
{
    const auto mesh = SkinningTests::MakeMesh(129);
    ASSERT_TRUE(mesh);
    const auto firstHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(firstHandle);
    auto& first = *firstHandle;
    const auto secondHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    auto pose = second.GetLocalPose();
    pose.LocalTransforms[1].Translation = {5, 2, -3};
    ASSERT_TRUE(second.SetLocalPose(pose));
    SkinningBatch batch;
    ASSERT_TRUE(batch.Append(*mesh, first.GetModelPose(), 7, 17, 4));
    ASSERT_TRUE(batch.Append(*mesh, second.GetModelPose(), 7, 146, 4));
    ASSERT_EQ(batch.GetJobs().size(), 2);
    ASSERT_EQ(batch.GetBlocks().size(), 4);
    EXPECT_EQ(batch.GetJobs()[1].BoneMatrixOffset, 2);
    EXPECT_EQ(batch.GetJobs()[1].SourceVertexOffset, 7);
    EXPECT_EQ(batch.GetJobs()[1].OutputVertexOffset, 146);
    EXPECT_EQ(batch.GetBlocks()[1].FirstVertex, 128);
    EXPECT_EQ(batch.GetBlocks()[2].JobIndex, 1);
    EXPECT_EQ(batch.GetBlocks()[2].FirstVertex, 0);
    const auto expected = second.GetModelPose().Matrices[1] * mesh->GetData().InverseBindMatrices[0];
    EXPECT_EQ(batch.GetMatrices()[2], expected);
    EXPECT_NE(batch.GetMatrices()[0], batch.GetMatrices()[2]);
}

TEST(SkinningBatchTest, RejectsCapacityOverflowAndInvalidPosesWithoutChangingTheBatch)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto instanceHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    SkinningBatch batch;
    ASSERT_TRUE(batch.Append(*mesh, instance.GetModelPose(), 0, 0, 2));
    EXPECT_FALSE(batch.Append(*mesh, instance.GetModelPose(), 0, 3, 3));
    EXPECT_FALSE(batch.Append(*mesh, {}, 0, 3, 10));
    EXPECT_FALSE(batch.Append(*mesh, instance.GetModelPose(), UINT32_MAX, 0, 10));
    auto pose = instance.GetModelPose();
    pose.Matrices[1][0][0] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(batch.Append(*mesh, pose, 0, 3, 10));
    EXPECT_EQ(batch.GetJobs().size(), 1);
    EXPECT_EQ(batch.GetMatrices().size(), 2);
    EXPECT_EQ(batch.GetBlocks().size(), 1);
    batch.Reset();
    EXPECT_TRUE(batch.GetMatrices().empty());
    EXPECT_TRUE(batch.GetJobs().empty());
    EXPECT_TRUE(batch.GetBlocks().empty());
}

TEST(SkinningBatchTest, ManyCharactersShareSourceAndUseDistinctOutputRanges)
{
    const auto mesh = SkinningTests::MakeMesh(300);
    ASSERT_TRUE(mesh);
    const auto instanceHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    SkinningBatch batch;
    for (uint32_t i = 0; i < 1000; ++i)
        ASSERT_TRUE(batch.Append(*mesh, instance.GetModelPose(), 123, i * 300, 2000));
    EXPECT_EQ(batch.GetJobs().size(), 1000);
    EXPECT_EQ(batch.GetMatrices().size(), 2000);
    EXPECT_EQ(batch.GetBlocks().size(), 3000);
    EXPECT_EQ(batch.GetJobs().back().OutputVertexOffset, 299700);
    EXPECT_EQ(batch.GetJobs().back().SourceVertexOffset, 123);
}
