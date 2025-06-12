#pragma once

#include <Engine/Graphics/Buffer.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir
{
    class ELIXIR_API VulkanBaseBuffer : virtual public GraphicsBuffer
    {
      public:
        virtual ~VulkanBaseBuffer();

        virtual void Destroy();

        VkBuffer GetVulkanBuffer() const { return m_Buffer; }
        const VmaAllocationInfo& GetAllocationInfo() const { return m_AllocationInfo; }

        static Ref<VulkanBaseBuffer> CreateBuffer(
            const GraphicsContext* context,
            size_t size,
            VkBufferUsageFlags usage,
            VmaMemoryUsage memoryUsage
        );

      protected:
        VulkanBaseBuffer(
            const GraphicsContext* context,
            size_t size,
            VkBufferUsageFlags usage,
            VmaMemoryUsage memoryUsage
        );

        virtual void Init(VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

        VkBuffer m_Buffer;
        VmaAllocation m_Allocation;
        VmaAllocationInfo m_AllocationInfo;

        bool m_Destroyed;
    };

    class ELIXIR_API VulkanVertexBuffer : public VertexBuffer, public VulkanBaseBuffer
    {
      public:
        VulkanVertexBuffer(const GraphicsContext* context, size_t size);
        VulkanVertexBuffer(const GraphicsContext* context, void* data, size_t size);
        ~VulkanVertexBuffer() override = default;
    };

    class ELIXIR_API VulkanIndexBuffer : public IndexBuffer, public VulkanBaseBuffer
    {
      public:
        VulkanIndexBuffer(
            const GraphicsContext* context,
            void* data,
            size_t size,
            EIndexType type
        );
        
        ~VulkanIndexBuffer() override = default;
    };
}