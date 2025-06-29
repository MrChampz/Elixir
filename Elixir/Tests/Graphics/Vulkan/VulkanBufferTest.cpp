#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
using namespace Elixir::Vulkan;

class VulkanBufferTest : public Test
{
  protected:
    static void SetUpTestSuite()
    {
        Memory::s_Malloc = CreateScope<SystemMalloc>();
        Window = Window::Create();
        Context = GraphicsContext::Create(EGraphicsAPI::Vulkan, Window.get());
        Context->Init();
    }

    static void TearDownTestSuite()
    {
        Context->Shutdown();
    }

    static Scope<Window> Window;
    static Scope<GraphicsContext> Context;
};

Scope<Window> VulkanBufferTest::Window = nullptr;
Scope<GraphicsContext> VulkanBufferTest::Context = nullptr;

TEST_F(VulkanBufferTest, VulkanBaseBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<VulkanBaseBuffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanBaseBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanBaseBuffer>);
    EXPECT_FALSE(std::is_move_constructible_v<VulkanBaseBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanBaseBuffer>);
}

TEST_F(VulkanBufferTest, VulkanDynamicBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<VulkanDynamicBuffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanDynamicBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanDynamicBuffer>);
    EXPECT_FALSE(std::is_move_constructible_v<VulkanDynamicBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanDynamicBuffer>);
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

    const auto buffer = Buffer::Create(Context.get(), info);

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

    const auto staging = StagingBuffer::Create(Context.get(), info.Buffer.Size);
    const auto target = Buffer::Create(Context.get(), info);

    const auto cmd = Context->GetCommandBuffer();
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

    const auto buffer = Buffer::Create(Context.get(), info);

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

    const auto buffer = StagingBuffer::Create(Context.get(), size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_EQ(buffer->GetUsage(), EBufferUsage::TransferSrc);

    void* mapped = buffer->Map();
    ASSERT_NE(mapped, nullptr);

    EXPECT_EQ(std::memcmp(mapped, data.data(), size), 0)
        << "Buffer contents do not match expected data.";

    buffer->Unmap();
    buffer->Destroy();

    EXPECT_TRUE(buffer->IsDestroyed());
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
    const std::vector<uint8_t> data(size, 0xAA);

    const auto buffer = VertexBuffer::Create(Context.get(), size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_GT(buffer->GetAddress(), 0);

    buffer->Destroy();

    EXPECT_TRUE(buffer->IsDestroyed());
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
        Context.get(),
        size,
        data.data(),
        EIndexType::UInt32
    );

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_EQ(buffer->GetIndexType(), EIndexType::UInt32);

    buffer->Destroy();

    EXPECT_TRUE(buffer->IsDestroyed());
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

    const auto buffer = UniformBuffer::Create(Context.get(), size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_EQ(buffer->GetUsage(), EBufferUsage::UniformBuffer);

    void* mapped = buffer->Map();
    ASSERT_NE(mapped, nullptr);

    EXPECT_EQ(std::memcmp(mapped, data.data(), size), 0)
        << "Buffer contents do not match expected data.";

    buffer->Unmap();
    buffer->Destroy();

    EXPECT_TRUE(buffer->IsDestroyed());
}