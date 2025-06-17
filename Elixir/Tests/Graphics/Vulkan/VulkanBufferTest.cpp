#include <gtest/gtest.h>
using namespace testing;

#include <Graphics/Vulkan/VulkanBuffer.h>
using namespace Elixir::Vulkan;

class VulkanBufferTest : public Test
{
  protected:
    static void SetUpTestSuite()
    {
        Window = Window::Create();
        Context = GraphicsContext::Create(EGraphicsAPI::Vulkan, Window.get());
        Context->Init();
    }

    static Scope<Window> Window;
    static Scope<GraphicsContext> Context;
};

Scope<Window> VulkanBufferTest::Window = nullptr;
Scope<GraphicsContext> VulkanBufferTest::Context = nullptr;

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

    SUCCEED();
}

TEST_F(VulkanBufferTest, VulkanVertexBuffer_CreationAndAddress)
{
    constexpr size_t size = 512;
    const std::vector<uint8_t> data(size, 0xAA);

    const auto buffer = VertexBuffer::Create(Context.get(), size, data.data());

    EXPECT_EQ(buffer->GetSize(), size);
    EXPECT_GT(buffer->GetAddress(), 0);

    buffer->Destroy();
    SUCCEED();
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
    SUCCEED();
}