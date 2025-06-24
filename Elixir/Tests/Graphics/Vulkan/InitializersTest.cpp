#include <gtest/gtest.h>

#include <Engine/Graphics/Buffer.h>
#include <Graphics/Vulkan/Initializers.h>
using namespace Elixir;
using namespace Elixir::Vulkan::Initializers;

TEST(InitializersTest_AllocationCreateInfo, SetsRequiredAndPreferredFlags)
{
    SAllocationInfo info;
    info.RequiredFlags = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent;
    info.PreferredFlags = EMemoryProperty::DeviceLocal;

    VmaAllocationCreateInfo allocInfo = AllocationCreateInfo(info);

    EXPECT_EQ(allocInfo.requiredFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EXPECT_EQ(allocInfo.preferredFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

TEST(InitializersTest_AllocationCreateInfo, ZeroFlags)
{
    SAllocationInfo info;
    info.RequiredFlags = (EMemoryProperty)0;
    info.PreferredFlags = (EMemoryProperty)0;

    VmaAllocationCreateInfo allocInfo = AllocationCreateInfo(info);

    EXPECT_EQ(allocInfo.requiredFlags, 0u);
    EXPECT_EQ(allocInfo.preferredFlags, 0u);
}

TEST(InitializersTest_BufferCreateInfo, SetsCorrectFields)
{
    SBufferCreateInfo info;
    info.Buffer.Size = 1024;
    info.Usage = EBufferUsage::VertexBuffer;

    VkBufferCreateInfo bufferInfo = BufferCreateInfo(info);

    EXPECT_EQ(bufferInfo.sType, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    EXPECT_EQ(bufferInfo.pNext, nullptr);
    EXPECT_EQ(bufferInfo.size, (VkDeviceSize)info.Buffer.Size);
    EXPECT_EQ(bufferInfo.usage, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);



}

TEST(InitializersTest_BufferCreateInfo, ZeroSize)
{
    SBufferCreateInfo info = {};
    info.Buffer.Size = 0;
    info.Usage = (EBufferUsage)0;

    VkBufferCreateInfo bufferInfo = BufferCreateInfo(info);

    EXPECT_EQ(bufferInfo.size, 0u);
    EXPECT_EQ(bufferInfo.usage, 0u);
}