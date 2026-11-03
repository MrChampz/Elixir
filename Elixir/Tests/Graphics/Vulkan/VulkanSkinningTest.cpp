#include "../../Engine/Animation/AnimationTestNodes.h"
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/AnimationManager.h>
#include <Engine/Core/Timer.h>
#include <Engine/Graphics/RenderableSnapshot.h>
#include <Engine/Animation/TwoBoneIK.h>
#include "../../Engine/Animation/AnimationTestPlayback.h"
#include <Engine/Animation/BlendSpace2D.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <gtest/gtest.h>
#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Mesh/SkinningPass.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
#include "VulkanTestContext.h"
#include "../../Engine/Mesh/SkinningTestData.h"

#include <chrono>
#include <future>
#include <thread>

using namespace Elixir;

TEST(VulkanSkinningTest, BatchedComputeMatchesCpuWithSharedSourceAndIndependentPoses)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        GeometryPool source(*context, {300, 12, sizeof(SSkeletalMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        GeometryPool output(*context, {300, 0, sizeof(SStaticMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
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
            const auto firstInstanceHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
            ASSERT_TRUE(firstInstanceHandle);
            auto& firstInstance = *firstInstanceHandle;
            const auto secondInstanceHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
            ASSERT_TRUE(secondInstanceHandle);
            auto& secondInstance = *secondInstanceHandle;
            if (frame > 0)
            {
                auto animation = Sequence::Create("Blend", mesh->GetSkeleton(), 2, {
                    { .BoneIndex = 0, .Translations = {
                        { 0, { 2, 1, -1 } }, { 2, { 4, 3, -1 } },
                      },
                      .Rotations = {{ 0, glm::angleAxis(1.0f, glm::vec3(0, 0, 1)) }},
                      .Scales = {{ 0, { 2, 3, 1 } }} },
                }, {{ "LeftFootPlant", 0.2f }, { "RightFootPlant", 1.2f }});
                const auto reference = Sequence::Create(
                    "Reference", mesh->GetSkeleton(), 1, {},
                    {{ "LeftFootPlant", 0.25f }, { "RightFootPlant", 0.75f }}
                );
                const auto space = BlendSpace1D::Create("Locomotion", {
                    { 0.0f, reference }, { 1.0f, animation },
                });
                if (frame == 3)
                {
                    ASSERT_TRUE(firstInstance.SetRootMotionEnabled(true));
                    const auto lateral = Sequence::Create("Lateral", mesh->GetSkeleton(),
                        3, {{ .BoneIndex = 1, .Translations = {
                            { 0, { -2, 1, 3 } }, { 3, { 1, 4, 3 } },
                        } }}, {{ "RightFootPlant", 0.3f }, { "LeftFootPlant", 1.8f }});
                    const auto plane = BlendSpace2D::Create("Plane", {
                        {{ 0, 0 }, reference }, {{ 1, 0 }, animation }, {{ 0, 1 }, lateral },
                    });
                    ASSERT_TRUE(PlayWithInputs(firstInstance, MakeNode(plane), { 0.25f, 0.25f }, 2.0f));
                }
                else
                    ASSERT_TRUE(PlayWithInputs(firstInstance, MakeNode(space), { 0.5f }, 2.0f));
                ASSERT_TRUE(firstInstance.Update(float(frame) * 0.5f));
                ASSERT_TRUE(firstInstance.IsTransitioning());
            }
            auto pose = secondInstance.GetLocalPose();
            pose.LocalTransforms[0].Scale = {-2, 3, 0.5f};
            pose.LocalTransforms[0].Translation = {float(frame), 2, -1};
            pose.LocalTransforms[1].Translation = {3, -2, float(frame)};
            pose.LocalTransforms[1].Rotation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1, 2, 3)));
            ASSERT_TRUE(secondInstance.SetLocalPose(pose));
            if (frame > 0)
            {
                auto mask = BoneMask::FromBone("Action", mesh->GetSkeleton(),
                    mesh->GetSkeleton()->GetBones()[1].Name);
                auto action = Sequence::Create("Layer", mesh->GetSkeleton(), 1, {
                    { .BoneIndex = 1, .Translations = {
                        { 0, { 1, 2, 3 } }, { 1, { 3, 4, 5 } },
                    } },
                });
                action = action->WithNotifies({{"Active", 0, 1}});
                ASSERT_TRUE(action);
                ASSERT_TRUE(firstInstance.AddLayer("Action", mask, 0.65f));
                if (frame == 2)
                    ASSERT_TRUE(firstInstance.GetLayer("Action")->SetBlendMode(
                        EAnimationLayerBlendMode::Additive));
                auto& player = firstInstance.GetLayer("Action")->GetPlayer();
                ASSERT_TRUE(player.Play(MakeNode(action)));
                const auto montage = AnimationMontage::Create({
                    .Name = "ActionMontage", .Sequence = action,
                    .BlendIn = 0.25f, .BlendOut = 0.25f,
                    .Sections = {{"Start", 0, "End"}, {"Skip", 0.25f},
                        {"End", 0.75f, ""}}
                });
                ASSERT_TRUE(montage);
                ASSERT_TRUE(player.PlayMontage(montage));
                ASSERT_TRUE(firstInstance.Update(frame == 3 ? 0.375f : 0.125f));
                EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-6f);
                EXPECT_EQ(firstInstance.GetNotifyEvents().size(), frame == 3 ? 5u : 2u);
                if (frame == 3)
                    EXPECT_EQ(player.GetCurrentMontageSection(), "End");
                if (frame == 3)
                {
                    EXPECT_EQ(firstInstance.GetLocalPose().LocalTransforms[0].Translation,
                        mesh->GetData().ReferencePose.LocalTransforms[0].Translation);
                    EXPECT_TRUE(firstInstance.GetRootMotionDelta().IsValid());
                }
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
        auto reused = output.Allocate(129);
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

TEST(VulkanSkinningTest, VertexOnlyGeometryWorksWithAndWithoutAnIndexBuffer)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        for (const uint32_t indexCapacity : {0u, 3u})
        {
            SCOPED_TRACE(indexCapacity);
            GeometryPool pool(*context, {6, indexCapacity, sizeof(SStaticMeshVertex)});
            ASSERT_TRUE(pool.IsValid());
            EXPECT_EQ(bool(pool.GetIndexBuffer()), indexCapacity > 0);

            SStaticMeshData data;
            data.Vertices.resize(3);
            data.Vertices[0].Position = {1, 2, 3};
            auto uploaded = pool.Upload(data);
            ASSERT_TRUE(uploaded.IsValid());
            EXPECT_EQ(uploaded.Get()->VertexOffset, 0);
            EXPECT_EQ(uploaded.Get()->IndexCount, 0);

            auto info = StagingBuffer::CreateBufferInfo(
                3 * sizeof(SStaticMeshVertex),
                nullptr
            );
            info.Usage = EBufferUsage::TransferDst;
            const Ref<StagingBuffer> readback =
                CreateRef<Vulkan::VulkanStagingBuffer>(context, info);
            ASSERT_TRUE(readback->IsValid());
            const auto cmd = context->GetUploadCommandBuffer();
            cmd->Begin();
            pool.GetVertexBuffer()->Barrier(
                cmd,
                EPipelineStage::Transfer,
                EPipelineAccess::TransferRead
            );
            SBufferCopy copy{.Size = 3 * sizeof(SStaticMeshVertex)};
            pool.GetVertexBuffer()->Copy(cmd, readback, std::span{&copy, 1});
            cmd->Flush();
            const auto* vertices = static_cast<const SStaticMeshVertex*>(readback->Map());
            ASSERT_NE(vertices, nullptr);
            EXPECT_EQ(vertices[0].Position, data.Vertices[0].Position);
            readback->Unmap();

            if (indexCapacity == 0)
            {
                EXPECT_FALSE(pool.Allocate(3, 1).IsValid());
                data.Indices = {0, 1, 2};
                EXPECT_FALSE(pool.Upload(data).IsValid());
            }
            auto remaining = pool.Allocate(3, indexCapacity);
            ASSERT_TRUE(remaining.IsValid());
            EXPECT_EQ(remaining.Get()->VertexOffset, 3);
            EXPECT_FALSE(pool.Allocate(1).IsValid());

            remaining.Reset();
            context->WaitForSubmittedFrames();
            auto released = pool.Allocate(3, indexCapacity);
            ASSERT_TRUE(released.IsValid());
            EXPECT_EQ(released.Get()->VertexOffset, 3);
            EXPECT_EQ(released.Get()->IndexOffset, 0);

            pool.Reset();
            EXPECT_FALSE(uploaded.IsValid());
            EXPECT_FALSE(released.IsValid());
            auto all = pool.Allocate(6, indexCapacity);
            ASSERT_TRUE(all.IsValid());
            EXPECT_EQ(all.Get()->VertexOffset, 0);
            EXPECT_EQ(all.Get()->IndexOffset, 0);
        }

        const auto mesh = SkinningTests::MakeMesh();
        ASSERT_TRUE(mesh);
        GeometryPool skeletal(*context, {3, 0, sizeof(SSkeletalMeshVertex)});
        auto data = mesh->GetData();
        data.Indices.clear();
        auto uploaded = skeletal.Upload(data);
        ASSERT_TRUE(uploaded.IsValid());
        EXPECT_EQ(uploaded.Get()->IndexCount, 0);
        EXPECT_FALSE(skeletal.GetIndexBuffer());
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
        const auto instanceHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
        ASSERT_TRUE(instanceHandle);
        auto& instance = *instanceHandle;
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
        GeometryPool output(*context, {vertexCount * instanceCount, 0, sizeof(SStaticMeshVertex), EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        auto geometry = source.Upload(mesh->GetData());
        auto result = output.Allocate(vertexCount * instanceCount);
        ASSERT_TRUE(geometry.IsValid());
        ASSERT_TRUE(result.IsValid());
        SkinningBatch batch;
        std::vector<glm::vec3> expected;
        expected.reserve(instanceCount);
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            const auto instanceHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
            ASSERT_TRUE(instanceHandle);
            auto& instance = *instanceHandle;
            auto pose = instance.GetLocalPose();
            pose.LocalTransforms[0].Translation = {float(i), float(i % 13), -float(i % 7)};
            pose.LocalTransforms[1].Translation = {0, float(i % 5), 1};
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

TEST(VulkanSkinningTest, TwoBoneIKPoseReachesComputeSkinning)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    ASSERT_TRUE(context->RunRenderTaskAndWait([&]
    {
        auto data = SkinningTests::MakeMesh()->GetData();
        data.Skeleton = Skeleton::Create("Limb", {
            {.Name = "Scene"}, {.Name = "Root", .ParentIndex = 0},
            {.Name = "Middle", .ParentIndex = 1,
                .ReferenceTransform = {.Translation = {1, 0, 0}}},
            {.Name = "End", .ParentIndex = 2,
                .ReferenceTransform = {.Translation = {1, 0, 0}}},
            {.Name = "Target", .ParentIndex = 0,
                .ReferenceTransform = {.Translation = {1, 0, 0}}}
        });
        data.ReferencePose.LocalTransforms.clear();
        data.BoneMap = {3};
        data.InverseBindMatrices = {glm::translate(glm::mat4(1), glm::vec3(-2, 0, 0))};
        for (auto& vertex : data.Vertices)
        {
            vertex.Joints = {};
            vertex.Weights = {1, 0, 0, 0};
        }
        data.Vertices[0].Position = {2, 0, 0};
        data.Vertices[1].Position = {1, 0, 0};
        const auto mesh = SkeletalMesh::Create(std::move(data));
        ASSERT_TRUE(mesh);
        const auto animation = Animation::Create(
            mesh->GetSkeleton(),
            mesh->GetData().ReferencePose
        );
        ASSERT_TRUE(animation);
        const auto targetAnimation = Sequence::Create(
            "Target",
            mesh->GetSkeleton(),
            1,
            {{
                .BoneIndex = 4, .Translations = {{0, {1, 0, 0}}, {1, {1, 2, 0}}},
                .Rotations = {{0, glm::angleAxis(0.5f, glm::vec3(0, 0, 1))}}
            }}
        );
        ASSERT_TRUE(animation->Play(SequenceNode::Create(targetAnimation), false));
        const STwoBoneIKConstraint constraint{
            3, {}, {0, 2, 0}, 1,
            glm::angleAxis(glm::radians(90.0f) - 0.5f, glm::vec3(0, 0, 1)), uint32_t(4)
        };
        const auto ik = TwoBoneIK::Create(mesh->GetSkeleton());
        ASSERT_TRUE(ik);
        ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
        const std::array<Ref<const PoseModifier>, 1> modifiers{ik};
        ASSERT_TRUE(animation->SetPoseModifiers(modifiers));
        ASSERT_TRUE(animation->Update(0.5f));
        std::vector<SStaticMeshVertex> expected;
        ASSERT_TRUE(mesh->SkinVertices(animation->GetModelPose(), expected));
        EXPECT_NEAR(expected[0].Position.x, 1, 1e-5f);
        EXPECT_NEAR(expected[0].Position.y, 1, 1e-5f);
        EXPECT_NEAR(expected[1].Position.x, 1, 1e-5f);
        EXPECT_NEAR(expected[1].Position.y, 0, 1e-5f);
        GeometryPool source(*context, {3, 3, sizeof(SSkeletalMeshVertex),
            EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        GeometryPool output(*context, {3, 3, sizeof(SStaticMeshVertex),
            EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer});
        auto geometry = source.Upload(mesh->GetData());
        auto result = output.Allocate(3);
        ASSERT_TRUE(geometry.IsValid());
        ASSERT_TRUE(result.IsValid());
        SkinningBatch batch;
        ASSERT_TRUE(batch.Append(*mesh, animation->GetModelPose(),
            geometry.Get()->VertexOffset, result.Get()->VertexOffset, 3));
        ShaderLoader loader(context);
        SkinningPass pass(*context, loader);
        auto info = StagingBuffer::CreateBufferInfo(output.GetVertexBuffer()->GetSize(), nullptr);
        info.Usage = EBufferUsage::TransferDst;
        Ref<StagingBuffer> readback = CreateRef<Vulkan::VulkanStagingBuffer>(context, info);
        const auto cmd = context->GetUploadCommandBuffer();
        cmd->Begin();
        ASSERT_TRUE(pass.Record(cmd, batch, source.GetVertexBuffer(), output.GetVertexBuffer(),
            EPipelineStage::Transfer, EPipelineAccess::TransferRead));
        output.GetVertexBuffer()->Copy(cmd, readback);
        cmd->Flush();
        const auto* actual = static_cast<const SStaticMeshVertex*>(readback->Map());
        for (size_t i = 0; i < expected.size(); ++i)
            for (int axis = 0; axis < 3; ++axis)
                EXPECT_NEAR(actual[i].Position[axis], expected[i].Position[axis], 1e-5f);
        readback->Unmap();
    }));
}

TEST(VulkanSkinningTest, GameThreadUpdatesWhileRenderingAnEarlierPoseSnapshot)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto source = Sequence::Create(
        "Move",
        mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {2, 0, 0}}}}}
    );
    ASSERT_TRUE(source);
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    const auto instance = SkeletalMeshInstance::Create(mesh);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(animation->Play(MakeNode(source)));
    const SRenderableSnapshot snapshot{
        .Transform = glm::mat4(1),
        .Data = SSkeletalMeshRenderData{instance, animation->GetFrameSnapshot()}
    };
    const auto previousPose = std::get<SSkeletalMeshRenderData>(snapshot.Data)
        .Frame->ModelPose.Matrices;
    std::vector<glm::mat4> renderedPose;
    std::promise<void> readerStarted;
    auto started = readerStarted.get_future();
    std::promise<void> releaseReader;
    auto released = releaseReader.get_future();
    std::promise<void> readerFinished;
    auto finished = readerFinished.get_future();
    bool readerReleased = false;
    const auto readPose = [&, snapshot]
    {
        readerStarted.set_value();
        const auto status = released.wait_for(std::chrono::seconds(2));
        readerReleased = status == std::future_status::ready;
        renderedPose = std::get<SSkeletalMeshRenderData>(snapshot.Data)
            .Frame->ModelPose.Matrices;
        readerFinished.set_value();
    };
    ASSERT_TRUE(context->EnqueueRenderTask(readPose));
    EXPECT_EQ(started.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_TRUE(AnimationManager::Get().Update(Timestep(0.25f)));
    EXPECT_EQ(finished.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
    EXPECT_NE(animation->GetModelPose().Matrices, previousPose);
    EXPECT_FLOAT_EQ(animation->GetTime(), 0.25f);
    releaseReader.set_value();
    EXPECT_EQ(finished.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const auto completed = context->RunRenderTaskAndWait([] {});
    ASSERT_TRUE(completed);
    EXPECT_TRUE(readerReleased);
    EXPECT_EQ(renderedPose, previousPose);
}

TEST(VulkanSkinningTest, RenderQueueConsumesTransferredAnimationFramesInOrder)
{
    auto* context = VulkanTestContext::Get().GetGraphicsContext();
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto source = Sequence::Create(
        "Move",
        mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {2, 0, 0}}}}}
    );
    ASSERT_TRUE(source);
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    ASSERT_TRUE(animation);
    ASSERT_TRUE(animation->Play(MakeNode(source)));
    const auto first = animation->GetFrameSnapshot();
    Ref<const SAnimationFrame> renderOwned;
    std::vector<Ref<const SAnimationFrame>> rendered;
    const auto transferFirst = [&, first] { renderOwned = first; };
    EXPECT_TRUE(context->EnqueueRenderTask(transferFirst));
    EXPECT_TRUE(animation->Update(0.25f));
    const auto second = animation->GetFrameSnapshot();
    const auto draw = [&]
    {
        EXPECT_TRUE(context->IsRenderThread());
        rendered.push_back(renderOwned);
    };
    EXPECT_TRUE(context->EnqueueRenderTask(draw));
    const auto transferSecond = [&, second] { renderOwned = second; };
    EXPECT_TRUE(context->EnqueueRenderTask(transferSecond));
    EXPECT_TRUE(animation->Update(0.25f));
    EXPECT_TRUE(context->EnqueueRenderTask(draw));
    const auto completed = context->RunRenderTaskAndWait([] {});
    ASSERT_TRUE(completed);
    ASSERT_EQ(rendered.size(), 2);
    ASSERT_TRUE(rendered[0]);
    ASSERT_TRUE(rendered[1]);
    EXPECT_EQ(rendered[0], first);
    EXPECT_EQ(rendered[1], second);
    EXPECT_NE(rendered[1], animation->GetFrameSnapshot());
    EXPECT_NE(rendered[0]->ModelPose.Matrices, rendered[1]->ModelPose.Matrices);
}
