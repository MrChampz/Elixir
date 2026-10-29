#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Core/Executor/Executor.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Graphics/Vulkan/VulkanBuffer.h>

#include "VulkanTestContext.h"

using namespace Elixir::Vulkan;

class VulkanBufferTest : public Test
{
  protected:
    static void SetUpTestSuite()
    {
        Context = VulkanTestContext::Get().GetGraphicsContext();
    }

    static GraphicsContext* Context;
};

GraphicsContext* VulkanBufferTest::Context = nullptr;

TEST_F(VulkanBufferTest, DefaultCopyRetainsItsRegionUntilCommandsAreRecorded)
{
    auto copy = []
    {
        std::array<uint32_t, 32> expected{};
        for (uint32_t i = 0; i < expected.size(); ++i)
            expected[i] = i * 17 + 3;
        const auto source = StagingBuffer::Create(Context, sizeof(expected), expected.data());
        auto info = StagingBuffer::CreateBufferInfo(sizeof(expected), nullptr);
        info.Usage = EBufferUsage::TransferDst;
        const Ref<StagingBuffer> destination = CreateRef<VulkanStagingBuffer>(Context, info);
        ASSERT_TRUE(source && destination);
        const auto cmd = Context->GetUploadCommandBuffer();
        cmd->Begin();
        source->Copy(cmd.get(), destination.get());
        cmd->Flush();
        const auto* values = static_cast<const uint32_t*>(destination->Map());
        ASSERT_NE(values, nullptr);
        for (size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(values[i], expected[i]);
        destination->Unmap();
    };
    const auto completed = Context->RunRenderTaskAndWait(copy);
    ASSERT_TRUE(completed);
}

TEST_F(VulkanBufferTest, GenericBuffersBindAsUniformAndStorageWithoutChangingDescriptorTypes)
{
    ASSERT_TRUE(Context->RunRenderTaskAndWait([&]
    {
        ShaderLoader loader(Context);
        auto shader = loader.LoadShader(
            ELIXIR_TEST_SHADER_DIR, "BufferBindings", EShaderStage::Compute
        );
        ASSERT_TRUE(shader);
        auto pipeline = ComputePipeline::Create(Context, { .Shader = shader });
        ASSERT_TRUE(pipeline);

        std::array<uint32_t, 4> values{13, 29, 0, 0};
        auto info = VertexBuffer::CreateBufferInfo(sizeof(values), values.data());
        info.Usage |= EBufferUsage::UniformBuffer | EBufferUsage::StorageBuffer;
        const Ref<Buffer> source = VertexBuffer::Create(Context, info);
        const auto output = StorageBuffer::Create(Context, sizeof(uint32_t));
        ASSERT_TRUE(source && source->IsValid());
        ASSERT_TRUE(output && output->IsValid());

        auto readbackInfo = StagingBuffer::CreateBufferInfo(sizeof(uint32_t), nullptr);
        readbackInfo.Usage = EBufferUsage::TransferDst;
        const Ref<StagingBuffer> readback = CreateRef<VulkanStagingBuffer>(Context, readbackInfo);
        shader->BindConstantBuffer("constants", source);
        shader->BindStorageBuffer("inputValues", source);
        shader->BindStorageBuffer("outputValues", output);

        const auto dispatch = [&]
        {
            const auto cmd = Context->GetUploadCommandBuffer();
            cmd->Begin();
            source->Barrier(cmd, EPipelineStage::ComputeShader,
                EPipelineAccess::UniformRead | EPipelineAccess::ShaderRead);
            output->Barrier(cmd, EPipelineStage::ComputeShader, EPipelineAccess::ShaderWrite);
            pipeline->Bind(cmd);
            cmd->Dispatch(1);
            output->Barrier(cmd, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
            output->Copy(cmd, readback);
            cmd->Flush();
            const auto result = *static_cast<const uint32_t*>(readback->Map());
            readback->Unmap();
            return result;
        };
        EXPECT_EQ(dispatch(), 42);

        shader->BindConstantBuffer("constants", output);
        shader->BindConstantBuffer("inputValues", source);
        shader->BindConstantBuffer("constants", nullptr);
        values[0] = 17;
        shader->SetConstantBuffer("constants", values.data(), sizeof(values));
        EXPECT_EQ(dispatch(), 42);

        const auto uniform = UniformBuffer::Create(Context, sizeof(values), values.data());
        ASSERT_TRUE(uniform && uniform->IsValid());
        shader->BindConstantBuffer("constants", uniform);
        values[0] = 23;
        shader->SetConstantBuffer("constants", values.data(), sizeof(values));
        EXPECT_EQ(dispatch(), 52);

        std::array<uint32_t, 8> oversized{};
        shader->SetConstantBuffer("constants", oversized.data(), sizeof(oversized));
        EXPECT_EQ(dispatch(), 52);

        const SBufferCreateInfo undersizedInfo{
            .Buffer = SBuffer(sizeof(uint32_t)),
            .Usage = EBufferUsage::UniformBuffer | EBufferUsage::TransferDst,
            .AllocationInfo = { .PreferredFlags = EMemoryProperty::DeviceLocal },
        };
        const auto undersized = Buffer::Create(Context, undersizedInfo);
        ASSERT_TRUE(undersized && undersized->IsValid());
        ASSERT_LT(undersized->GetSize(), sizeof(values));
        shader->BindConstantBuffer("constants", undersized);
        EXPECT_EQ(dispatch(), 52);

        shader = loader.LoadShader(ELIXIR_TEST_SHADER_DIR, "BufferBindings", EShaderStage::Compute);
        ASSERT_TRUE(shader);
        pipeline = ComputePipeline::Create(Context, { .Shader = shader });
        ASSERT_TRUE(pipeline);
        shader->BindStorageBuffer("inputValues", source);
        shader->BindStorageBuffer("outputValues", output);
        shader->SetConstantBuffer("constants", values.data(), sizeof(values));
        EXPECT_EQ(dispatch(), 52);
        values[0] = 31;
        shader->SetConstantBuffer("constants", values.data(), sizeof(values));
        EXPECT_EQ(dispatch(), 60);
    }));
}

TEST_F(VulkanBufferTest, VertexBufferSupportsExplicitStorageUsageWithoutChangingType)
{
    const auto vertex = VertexBuffer::Create(Context, 256);
    ASSERT_TRUE(vertex && vertex->IsValid());
    EXPECT_FALSE(HasAnyFlags(vertex->GetUsage(), EBufferUsage::StorageBuffer));

    auto info = VertexBuffer::CreateBufferInfo(256, nullptr);
    info.Usage |= EBufferUsage::StorageBuffer;
    const auto storageVertex = VertexBuffer::Create(Context, info);
    ASSERT_TRUE(storageVertex && storageVertex->IsValid());
    EXPECT_TRUE(HasAnyFlags(storageVertex->GetUsage(), EBufferUsage::StorageBuffer));
    EXPECT_TRUE(HasAnyFlags(storageVertex->GetUsage(), EBufferUsage::VertexBuffer));
    EXPECT_GT(storageVertex->GetAddress(), 0);

    info.Usage = EBufferUsage::StorageBuffer;
    EXPECT_FALSE(VertexBuffer::Create(Context, info));
}

TEST_F(VulkanBufferTest, VulkanBaseBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<VulkanBaseBuffer<Buffer>>);
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanBaseBuffer<Buffer>>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanBaseBuffer<Buffer>>);
    EXPECT_FALSE(std::is_move_constructible_v<VulkanBaseBuffer<Buffer>>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanBaseBuffer<Buffer>>);
}

TEST_F(VulkanBufferTest, VulkanDynamicBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<VulkanDynamicBuffer<StagingBuffer>>);
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanDynamicBuffer<StagingBuffer>>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanDynamicBuffer<StagingBuffer>>);
    EXPECT_FALSE(std::is_move_constructible_v<VulkanDynamicBuffer<StagingBuffer>>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanDynamicBuffer<StagingBuffer>>);
}

TEST_F(VulkanBufferTest, VulkanBuffer_CopyConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanBuffer>);
}

TEST_F(VulkanBufferTest, VulkanBuffer_MoveConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible_v<VulkanBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanBuffer>);
}

TEST_F(VulkanBufferTest, VulkanBuffer_CreationAndDestruction)
{
    SBufferCreateInfo info{};
    info.Buffer = SBuffer(256);
    info.Usage = EBufferUsage::TransferDst;
    info.AllocationInfo = {};

    const auto buffer = Buffer::Create(Context, info);

    EXPECT_EQ(buffer->GetSize(), 256);
    EXPECT_EQ(buffer->GetUsage(), EBufferUsage::TransferDst);

    buffer->Destroy();
    SUCCEED();
}

TEST_F(VulkanBufferTest, VulkanBuffer_DestroyAfterCopy)
{
    SBufferCreateInfo info{};
    info.Buffer = SBuffer(256);
    info.Usage = EBufferUsage::TransferDst;
    info.AllocationInfo = {};

    const auto staging = StagingBuffer::Create(Context, info.Buffer.Size);
    const auto target = Buffer::Create(Context, info);

    const auto cmd = Context->GetUploadCommandBuffer();
    cmd->Begin();
    staging->Copy(cmd, target);
    cmd->End();
    cmd->Flush();

    staging->Destroy();
    target->Destroy();

    SUCCEED();
}

TEST_F(VulkanBufferTest, VulkanBuffer_DoubleDestroyIsSafe)
{
    SBufferCreateInfo info{};
    info.Buffer = SBuffer(256);
    info.Usage = EBufferUsage::TransferDst;
    info.AllocationInfo = {};

    const auto buffer = Buffer::Create(Context, info);

    // First destroy
    buffer->Destroy();

    // Second destroy should be a no-op
    buffer->Destroy();

    SUCCEED();
}

TEST_F(VulkanBufferTest, VulkanStagingBuffer_CopyConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanStagingBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanStagingBuffer>);
}

TEST_F(VulkanBufferTest, VulkanStagingBuffer_MoveConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible_v<VulkanStagingBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanStagingBuffer>);
}

TEST_F(VulkanBufferTest, VulkanStagingBuffer_CreationAndMapping)
{
    constexpr size_t size = 128;
    const std::vector<uint8_t> data(size, 0xFF);

    const auto buffer = StagingBuffer::Create(Context, size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_EQ(buffer->GetUsage(), EBufferUsage::TransferSrc);

    void* mapped = buffer->Map();
    ASSERT_NE(mapped, nullptr);

    EXPECT_EQ(std::memcmp(mapped, data.data(), size), 0)
        << "Buffer contents do not match expected data.";

    buffer->Unmap();
    buffer->Destroy();

    EXPECT_FALSE(buffer->IsValid());
}

TEST_F(VulkanBufferTest, VulkanVertexBuffer_CopyConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanVertexBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanVertexBuffer>);
}

TEST_F(VulkanBufferTest, VulkanVertexBuffer_MoveConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible_v<VulkanVertexBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanVertexBuffer>);
}

TEST_F(VulkanBufferTest, VulkanVertexBuffer_CreationAndAddress)
{
    constexpr size_t size = 512;
    const std::vector data(size, (Byte)0xAA);

    const auto buffer = VertexBuffer::Create(Context, size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_GT(buffer->GetAddress(), 0);

    buffer->Destroy();

    EXPECT_FALSE(buffer->IsValid());
}

TEST_F(VulkanBufferTest, VulkanIndexBuffer_CopyConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanIndexBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanIndexBuffer>);
}

TEST_F(VulkanBufferTest, VulkanIndexBuffer_MoveConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible_v<VulkanIndexBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanIndexBuffer>);
}

TEST_F(VulkanBufferTest, VulkanIndexBuffer_CreationAndIndexType)
{
    constexpr size_t size = 256;
    const std::vector<uint32_t> data(size, 0xBB);

    const auto buffer = IndexBuffer::Create(
        Context,
        size,
        data.data(),
        EIndexType::UInt32
    );

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_EQ(buffer->GetIndexType(), EIndexType::UInt32);

    buffer->Destroy();

    EXPECT_FALSE(buffer->IsValid());
}

TEST_F(VulkanBufferTest, VulkanUniformBuffer_CopyConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanUniformBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanUniformBuffer>);
}

TEST_F(VulkanBufferTest, VulkanUniformBuffer_MoveConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible_v<VulkanUniformBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanUniformBuffer>);
}

TEST_F(VulkanBufferTest, VulkanUniformBuffer_CreationAndMapping)
{
    constexpr size_t size = 128;
    const std::vector<uint8_t> data(size, 0xFF);

    const auto buffer = UniformBuffer::Create(Context, size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_EQ(buffer->GetUsage(), EBufferUsage::UniformBuffer);

    void* mapped = buffer->Map();
    ASSERT_NE(mapped, nullptr);

    EXPECT_EQ(std::memcmp(mapped, data.data(), size), 0)
        << "Buffer contents do not match expected data.";

    buffer->Unmap();
    buffer->Destroy();

    EXPECT_FALSE(buffer->IsValid());
}
