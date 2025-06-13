#include "epch.h"
#include "VulkanBuffer.h"

namespace Elixir
{
    /* VulkanBaseBuffer */

    VulkanBaseBuffer::~VulkanBaseBuffer()
    {
        VulkanBaseBuffer::Destroy();
    }

    void VulkanBaseBuffer::Destroy()
    {

    }

    Ref<VulkanBaseBuffer> VulkanBaseBuffer::CreateBuffer(
        const GraphicsContext* context,
        const size_t size,
        const VkBufferUsageFlags usage,
        const VmaMemoryUsage memoryUsage
    )
    {
        return Ref<VulkanBaseBuffer>(new VulkanBaseBuffer(context, size, usage, memoryUsage));
    }

    VulkanBaseBuffer::VulkanBaseBuffer(
        const GraphicsContext* context,
        const size_t size,
        const VkBufferUsageFlags usage,
        const VmaMemoryUsage memoryUsage
    ) : GraphicsBuffer(context, size), m_Buffer(VK_NULL_HANDLE), m_Allocation(nullptr),
        m_Destroyed(false)
    {
        VulkanBaseBuffer::Init(usage, memoryUsage);
    }

    void VulkanBaseBuffer::Init(VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
    {

    }

    /* VulkanVertexBuffer */

    constexpr auto VERTEX_BUFFER_USAGE = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
									        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
									        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VulkanVertexBuffer::VulkanVertexBuffer(const GraphicsContext* context, const size_t size)
        : VulkanVertexBuffer(context, nullptr, size) {}

    VulkanVertexBuffer::VulkanVertexBuffer(
        const GraphicsContext* context,
        void* data,
        const size_t size
    ) : GraphicsBuffer(context, size), VertexBuffer(context, size),
        VulkanBaseBuffer(context, size, VERTEX_BUFFER_USAGE, VMA_MEMORY_USAGE_GPU_ONLY)
    {

    }

    /* VulkanIndexBuffer */

    constexpr auto INDEX_BUFFER_USAGE = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VulkanIndexBuffer::VulkanIndexBuffer(
        const GraphicsContext* context,
        void* data,
        const size_t size,
        const EIndexType type
    ) : GraphicsBuffer(context, size), IndexBuffer(context, size, type),
        VulkanBaseBuffer(context, size, INDEX_BUFFER_USAGE, VMA_MEMORY_USAGE_GPU_ONLY)
    {

    }
}