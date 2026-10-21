#include <Engine/Animation/BlendSpace2D.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <gtest/gtest.h>
#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Mesh/SkinningPass.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
#include "VulkanTestContext.h"
#include "../../Engine/Mesh/SkinningTestData.h"

using namespace Elixir;

TEST(VulkanSkinningTest, BatchedComputeMatchesCpuWithSharedSourceAndIndependentPoses)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        GeometryPool source(*context, {300, 12, sizeof(SSkeletalMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        GeometryPool output(*context, {300, 12, sizeof(SStaticMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        const auto prefixMesh = SkinningTests::MakeMesh();
        const auto mesh = SkinningTests::MakeMesh(129);
        ASSERT_TRUE(prefixMesh);
        ASSERT_TRUE(mesh);
        auto prefix = source.Upload(prefixMesh->GetData());
        auto geometry = source.Upload(mesh->GetData());
        ASSERT_TRUE(geometry.IsValid());
        EXPECT_EQ(geometry.Get()->VertexOffset, 3);
        auto sentinel = output.Allocate(9);
        auto first = output.Allocate(129);
        auto second = output.Allocate(129);
        ASSERT_TRUE(first.IsValid());
        ASSERT_TRUE(second.IsValid());
        EXPECT_NE(first.Get()->VertexOffset, second.Get()->VertexOffset);

        ShaderLoader loader(context);
        SkinningPass pass(*context, loader);
        ASSERT_TRUE(pass.IsValid());
        auto readbackInfo = StagingBuffer::CreateBufferInfo(output.GetVertexBuffer()->GetSize(), nullptr);
        readbackInfo.Usage = EBufferUsage::TransferDst;
        Ref<StagingBuffer> readback = CreateRef<Vulkan::VulkanStagingBuffer>(context, readbackInfo);

        for (int frame = 0; frame < 4; ++frame)
        {
            SkeletalMeshInstance firstInstance(mesh), secondInstance(mesh);
            if (frame > 0)
            {
                auto animation = AnimationSequence::Create("Blend", mesh->GetSkeleton(), 2, {
                    { .BoneIndex = 0, .Translations = {
                        { 0, { 2, 1, -1 } }, { 2, { 4, 3, -1 } },
                      },
                      .Rotations = {{ 0, glm::angleAxis(1.0f, glm::vec3(0, 0, 1)) }},
                      .Scales = {{ 0, { 2, 3, 1 } }} },
                }, {{ "LeftFootPlant", 0.2f }, { "RightFootPlant", 1.2f }});
                const auto reference = AnimationSequence::Create(
                    "Reference", mesh->GetSkeleton(), 1, {},
                    {{ "LeftFootPlant", 0.25f }, { "RightFootPlant", 0.75f }}
                );
                const auto space = BlendSpace1D::Create("Locomotion", {
                    { 0.0f, reference }, { 1.0f, animation },
                });
                if (frame == 3)
                {
                    const auto lateral = AnimationSequence::Create("Lateral", mesh->GetSkeleton(),
                        3, {{ .BoneIndex = 1, .Translations = {
                            { 0, { -2, 1, 3 } }, { 3, { 1, 4, 3 } },
                        } }}, {{ "RightFootPlant", 0.3f }, { "LeftFootPlant", 1.8f }});
                    const auto plane = BlendSpace2D::Create("Plane", {
                        {{ 0, 0 }, reference }, {{ 1, 0 }, animation }, {{ 0, 1 }, lateral },
                    });
                    ASSERT_TRUE(firstInstance.Play(plane, { 0.25f, 0.25f }, 2.0f));
                }
                else
                    ASSERT_TRUE(firstInstance.Play(space, { 0.5f }, 2.0f));
                ASSERT_TRUE(firstInstance.Update(float(frame) * 0.5f));
                ASSERT_TRUE(firstInstance.IsTransitioning());
            }
            auto pose = secondInstance.GetLocalPose();
            pose[0].Scale = {-2, 3, 0.5f};
            pose[0].Translation = {float(frame), 2, -1};
            pose[1].Translation = {3, -2, float(frame)};
            pose[1].Rotation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1, 2, 3)));
            ASSERT_TRUE(secondInstance.SetLocalPose(pose));
            if (frame > 0)
            {
                auto mask = BoneMask::FromBone("Action", mesh->GetSkeleton(),
                    mesh->GetSkeleton()->GetBones()[1].Name);
                auto action = AnimationSequence::Create("Layer", mesh->GetSkeleton(), 1, {
                    { .BoneIndex = 1, .Translations = {
                        { 0, { 1, 2, 3 } }, { 1, { 3, 4, 5 } },
                    } },
                });
                ASSERT_TRUE(firstInstance.AddLayer("Action", mask, 0.65f));
                if (frame == 2)
                    ASSERT_TRUE(firstInstance.GetLayer("Action")->SetBlendMode(
                        EAnimationLayerBlendMode::Additive));
                ASSERT_TRUE(firstInstance.GetLayer("Action")->Play(action));
                ASSERT_TRUE(firstInstance.Update(0.125f));
            }
            SkinningBatch batch;
            ASSERT_TRUE(batch.Append(*mesh, firstInstance.GetModelPose(), geometry.Get()->VertexOffset,
                first.Get()->VertexOffset, 4));
            ASSERT_TRUE(batch.Append(*mesh, secondInstance.GetModelPose(), geometry.Get()->VertexOffset,
                second.Get()->VertexOffset, 4));
            auto compute = context->GetSecondaryCommandBuffer();
            compute->Begin();
            ASSERT_TRUE(pass.Record(compute, batch,
                source.GetVertexBuffer(),
                output.GetVertexBuffer(),
                EPipelineStage::Transfer, EPipelineAccess::TransferRead));
            compute->End();
            const auto cmd = context->GetUploadCommandBuffer();
            cmd->Begin();
            cmd->ExecuteCommands(std::span{&compute, 1});
            output.GetVertexBuffer()->Copy(cmd, readback);
            cmd->Flush();

            const auto* vertices = static_cast<const SStaticMeshVertex*>(readback->Map());
            for (int instance = 0; instance < 2; ++instance)
            {
                std::vector<SStaticMeshVertex> expected;
                const auto& meshInstance = instance == 0 ? firstInstance : secondInstance;
                ASSERT_TRUE(mesh->SkinVertices(meshInstance.GetModelPose(), expected));
                const auto offset = instance == 0 ? first.Get()->VertexOffset : second.Get()->VertexOffset;
                for (uint32_t i = 0; i < 129; ++i)
                {
                    SCOPED_TRACE(::testing::Message() << "frame=" << frame << " instance=" << instance << " vertex=" << i);
                    const auto& actual = vertices[offset + i];
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        EXPECT_NEAR(actual.Position[axis], expected[i].Position[axis], 1e-4f);
                        EXPECT_NEAR(actual.Normal[axis], expected[i].Normal[axis], 1e-5f);
                        EXPECT_NEAR(actual.Tangent[axis], expected[i].Tangent[axis], 1e-5f);
                    }
                    EXPECT_FLOAT_EQ(actual.Tangent.w, expected[i].Tangent.w);
                    EXPECT_EQ(actual.TexCoord, expected[i].TexCoord);
                    EXPECT_EQ(actual.TexCoord1, expected[i].TexCoord1);
                }
            }
            readback->Unmap();
        }
        output.Reset();
        EXPECT_FALSE(first.IsValid());
        EXPECT_FALSE(second.IsValid());
        auto reused = output.Allocate(129, 3);
        ASSERT_TRUE(reused.IsValid());
        EXPECT_EQ(reused.Get()->VertexOffset, 0);
        EXPECT_EQ(reused.Get()->IndexOffset, 0);
    }));
}

TEST(VulkanSkinningTest, GeometryPoolRejectsMismatchedLayoutsAndExhaustion)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        GeometryPool pool(*context, {6, 3, sizeof(SStaticMeshVertex)});
        ASSERT_TRUE(pool.IsValid());
        EXPECT_TRUE(HasAnyFlags(pool.GetVertexBuffer()->GetUsage(), EBufferUsage::VertexBuffer));
        EXPECT_FALSE(HasAnyFlags(pool.GetVertexBuffer()->GetUsage(), EBufferUsage::StorageBuffer));
        auto mesh = SkinningTests::MakeMesh();
        ASSERT_TRUE(mesh);
        EXPECT_FALSE(pool.Upload(mesh->GetData()).IsValid());
        auto first = pool.Allocate(3, 3);
        ASSERT_TRUE(first.IsValid());
        EXPECT_FALSE(pool.Allocate(3, 1).IsValid());
        auto second = pool.Allocate(3);
        ASSERT_TRUE(second.IsValid());
        EXPECT_EQ(second.Get()->VertexOffset, 3);
        EXPECT_FALSE(pool.Allocate(1).IsValid());
        pool.Reset();
        EXPECT_FALSE(first.IsValid());
        EXPECT_FALSE(second.IsValid());
        auto all = pool.Allocate(6, 3);
        EXPECT_TRUE(all.IsValid());
    }));
}

TEST(VulkanSkinningTest, GeometryPoolsUseExplicitStorageBindings)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        GeometryPool storage(*context, {
            3, 3, sizeof(SSkeletalMeshVertex),
            EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer
        });
        ASSERT_TRUE(storage.IsValid());
        ASSERT_TRUE(storage.GetVertexBuffer());
        EXPECT_TRUE(HasAnyFlags(storage.GetVertexBuffer()->GetUsage(), EBufferUsage::VertexBuffer));
        EXPECT_TRUE(HasAnyFlags(storage.GetVertexBuffer()->GetUsage(), EBufferUsage::StorageBuffer));
        EXPECT_FALSE(HasAnyFlags(storage.GetVertexBuffer()->GetUsage(), EBufferUsage::IndexBuffer));

        const auto buffer = StorageBuffer::Create(context, 256);
        const auto dynamic = DynamicStorageBuffer::Create(context, 256);
        ASSERT_TRUE(buffer && buffer->IsValid());
        ASSERT_TRUE(dynamic && dynamic->IsValid());
        const auto geometryUsage = EBufferUsage::VertexBuffer | EBufferUsage::IndexBuffer;
        EXPECT_FALSE(HasAnyFlags(buffer->GetUsage(), geometryUsage));
        EXPECT_FALSE(HasAnyFlags(dynamic->GetUsage(), geometryUsage));

        auto info = StorageBuffer::CreateBufferInfo(256, nullptr);
        info.Usage = EBufferUsage::VertexBuffer;
        EXPECT_FALSE(StorageBuffer::Create(context, info));
        GeometryPool invalid(*context, {3, 3, sizeof(SStaticMeshVertex),
            EBufferUsage::StorageBuffer});
        EXPECT_FALSE(invalid.IsValid());
    }));
}

TEST(VulkanSkinningTest, RejectsVertexBuffersWithoutStorageUsage)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        GeometryPool source(*context, {3, 3, sizeof(SSkeletalMeshVertex)});
        GeometryPool output(*context, {3, 3, sizeof(SStaticMeshVertex)});
        ASSERT_TRUE(source.IsValid());
        ASSERT_TRUE(output.IsValid());
        const auto mesh = SkinningTests::MakeMesh();
        ASSERT_TRUE(mesh);
        SkeletalMeshInstance instance(mesh);
        SkinningBatch batch;
        ASSERT_TRUE(batch.Append(*mesh, instance.GetModelPose(), 0, 0, 2));
        ShaderLoader loader(context);
        SkinningPass pass(*context, loader);
        ASSERT_TRUE(pass.IsValid());
        const auto cmd = context->GetUploadCommandBuffer();
        cmd->Begin();
        EXPECT_FALSE(pass.Record(cmd, batch, source.GetVertexBuffer(), output.GetVertexBuffer()));
        cmd->End();
    }));
}

TEST(VulkanSkinningTest, ProcessesOneThousandIndependentCharactersInOneBatch)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        constexpr uint32_t instanceCount = 1000;
        constexpr uint32_t vertexCount = 129;
        const auto mesh = SkinningTests::MakeMesh(vertexCount);
        ASSERT_TRUE(mesh);
        GeometryPool source(*context, {vertexCount, 3, sizeof(SSkeletalMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        GeometryPool output(*context, {vertexCount * instanceCount, 3, sizeof(SStaticMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        auto geometry = source.Upload(mesh->GetData());
        auto result = output.Allocate(vertexCount * instanceCount);
        ASSERT_TRUE(geometry.IsValid());
        ASSERT_TRUE(result.IsValid());
        SkinningBatch batch;
        std::vector<glm::vec3> expected;
        expected.reserve(instanceCount);
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            SkeletalMeshInstance instance(mesh);
            auto pose = instance.GetLocalPose();
            pose[0].Translation = {float(i), float(i % 13), -float(i % 7)};
            pose[1].Translation = {0, float(i % 5), 1};
            ASSERT_TRUE(instance.SetLocalPose(pose));
            ASSERT_TRUE(batch.Append(*mesh, instance.GetModelPose(), geometry.Get()->VertexOffset,
                result.Get()->VertexOffset + i * vertexCount, instanceCount * 2));
            std::vector<SStaticMeshVertex> vertices;
            ASSERT_TRUE(mesh->SkinVertices(instance.GetModelPose(), vertices));
            expected.push_back(vertices.back().Position);
        }
        ASSERT_EQ(batch.GetJobs().size(), instanceCount);
        ASSERT_EQ(batch.GetBlocks().size(), instanceCount * 2);
        ShaderLoader loader(context);
        SkinningPass pass(*context, loader);
        auto info = StagingBuffer::CreateBufferInfo(output.GetVertexBuffer()->GetSize(), nullptr);
        info.Usage = EBufferUsage::TransferDst;
        Ref<StagingBuffer> readback = CreateRef<Vulkan::VulkanStagingBuffer>(context, info);
        const auto cmd = context->GetUploadCommandBuffer();
        cmd->Begin();
        ASSERT_TRUE(pass.Record(cmd, batch,
            source.GetVertexBuffer(),
            output.GetVertexBuffer(),
            EPipelineStage::Transfer, EPipelineAccess::TransferRead));
        output.GetVertexBuffer()->Copy(cmd, readback);
        cmd->Flush();
        const auto* vertices = static_cast<const SStaticMeshVertex*>(readback->Map());
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            SCOPED_TRACE(i);
            const auto& actual = vertices[(i + 1) * vertexCount - 1].Position;
            for (int axis = 0; axis < 3; ++axis)
                EXPECT_NEAR(actual[axis], expected[i][axis], 2e-4f);
        }
        readback->Unmap();
    }));
}
