#include <gtest/gtest.h>

#include <Graphics/Vulkan/Converters.h>
using namespace Elixir;
using namespace Elixir::Vulkan::Converters;

TEST(ConvertersTest_GetMemoryProperties, SingleFlag)
{
    EXPECT_EQ(GetMemoryProperties(EMemoryProperty::DeviceLocal), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    EXPECT_EQ(GetMemoryProperties(EMemoryProperty::HostVisible), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    EXPECT_EQ(GetMemoryProperties(EMemoryProperty::HostCoherent), VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EXPECT_EQ(GetMemoryProperties(EMemoryProperty::HostCached), VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    EXPECT_EQ(GetMemoryProperties(EMemoryProperty::LazilyAllocated), VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
}

TEST(ConvertersTest_GetMemoryProperties, MultipleFlags)
{
    const EMemoryProperty props = EMemoryProperty::DeviceLocal |
        EMemoryProperty::HostVisible |
        EMemoryProperty::HostCoherent;

    EXPECT_EQ(GetMemoryProperties(props),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

TEST(ConvertersTest_GetMemoryProperties, NoFlags)
{
    constexpr auto props = static_cast<EMemoryProperty>(0);
    EXPECT_EQ(GetMemoryProperties(props), 0u);
}

TEST(ConvertersTest_GetMemoryProperties, HasSameValueAsVulkan)
{
    const auto properties = EMemoryProperty::HostVisible |
        EMemoryProperty::HostCoherent;

    EXPECT_EQ(GetMemoryProperties(properties), 6);
}

TEST(ConvertersTest_GetBufferUsage, SingleFlag)
{
    EXPECT_EQ(GetBufferUsage(EBufferUsage::TransferSrc), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::TransferDst), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::UniformTexelBuffer), VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::StorageTexelBuffer), VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::StorageBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::IndexBuffer), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::VertexBuffer), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    EXPECT_EQ(GetBufferUsage(EBufferUsage::IndirectBuffer), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
}

TEST(ConvertersTest_GetBufferUsage, MultipleFlags)
{
    auto usage = EBufferUsage::TransferSrc | EBufferUsage::VertexBuffer;
    EXPECT_EQ(GetBufferUsage(usage), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    usage = EBufferUsage::UniformBuffer | EBufferUsage::StorageBuffer | EBufferUsage::IndexBuffer;
    EXPECT_EQ(GetBufferUsage(usage), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

TEST(ConvertersTest_GetBufferUsage, NoFlags)
{
    constexpr auto usage = static_cast<EBufferUsage>(0);
    EXPECT_EQ(GetBufferUsage(usage), 0u);
}

TEST(ConvertersTest_GetBufferUsage, HasSameValueAsVulkan)
{
    const auto usage = EBufferUsage::VertexBuffer |
        EBufferUsage::StorageBuffer |
        EBufferUsage::TransferDst |
        EBufferUsage::ShaderDeviceAddress;

    EXPECT_EQ(GetBufferUsage(usage), 131234);
}
