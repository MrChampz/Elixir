#include "epch.h"
#include "VulkanBuffer.h"

#include "VulkanCommandBuffer.h"

#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/Vulkan/Initializers.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    /* VulkanBaseBuffer */

    template <class T>
    void VulkanBaseBuffer<T>::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto derived = static_cast<T*>(this);
        if (derived->m_Destroyed) return;

        vmaDestroyBuffer(m_GraphicsContext->GetAllocator(), m_Buffer, m_Allocation);
        derived->m_Destroyed = true;
    }

    template <class T>
    void* VulkanBaseBuffer<T>::Map()
    {
        EE_PROFILE_ZONE_SCOPED()
        void* data;
        VK_CHECK_RESULT(vmaMapMemory(m_GraphicsContext->GetAllocator(), m_Allocation, &data));
        return data;
    }

    template <class T>
    void VulkanBaseBuffer<T>::Unmap()
    {
        EE_PROFILE_ZONE_SCOPED()
        vmaUnmapMemory(m_GraphicsContext->GetAllocator(), m_Allocation);
    }

    template <class T>
    void VulkanBaseBuffer<T>::Copy(
        const CommandBuffer* cmd,
        const Buffer* dst,
        const std::span<SBufferCopy> regions
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Dst = reinterpret_cast<const VulkanBaseBuffer*>(dst);

        std::span copyRegions = regions;
        std::array<SBufferCopy, 1> defaultRegion;

        if (copyRegions.empty())
        {
            defaultRegion = std::array<SBufferCopy, 1>({
                SBufferCopy{ .Size = static_cast<T*>(this)->GetSize() }
            });
            copyRegions = defaultRegion;
        }

        const auto srcBuffer = GetVulkanBuffer();
        const auto dstBuffer = vk_Dst->GetVulkanBuffer();

        vkCmdCopyBuffer(
            vk_Cmd->GetVulkanCommandBuffer(),
            srcBuffer,
            dstBuffer,
            copyRegions.size(),
            (const VkBufferCopy*)copyRegions.data()
        );
    }

    template <class T>
    VulkanBaseBuffer<T>::VulkanBaseBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : m_Buffer(VK_NULL_HANDLE), m_Allocation(nullptr)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);

        const auto derived = static_cast<T*>(this);
        derived->CreateBuffer(info);
        derived->InitBufferWithData(info.Buffer);
    }

    const auto VERTEX_BUFFER_USAGE = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
									 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    
    template <class T>
    void VulkanBaseBuffer<T>::CreateBuffer(const SBufferCreateInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto bufferInfo = Initializers::BufferCreateInfo(info);
        const auto allocInfo = Initializers::AllocationCreateInfo(info.AllocationInfo);
        
  //       VkBufferCreateInfo bufferInfo = {};
		// bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		// bufferInfo.pNext = nullptr;
		// bufferInfo.size = info.Buffer.Size;
		// bufferInfo.usage = VERTEX_BUFFER_USAGE;
        //usage 131234

		// VmaAllocationCreateInfo allocInfo = {};
		// allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		// allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        // flags 4
        // usage USAGE_GPU_ONLY
        // requiredFlags 4
        // preferredFlags 0

        m_Buffer = VK_NULL_HANDLE;

        VK_CHECK_RESULT(
            vmaCreateBuffer(
                m_GraphicsContext->GetAllocator(),
                &bufferInfo,
                &allocInfo,
                &m_Buffer,
                &m_Allocation,
                &m_AllocationInfo
            )
        );

        std::cout << "CreateBuffer" << std::endl;
    }

    template <class T>
    void VulkanBaseBuffer<T>::InitBufferWithData(const SBuffer& buffer)
    {
        if (buffer.Data)
        {
            auto& cmd = m_GraphicsContext->GetCommandBuffer();   // TODO: Should have a TRANSFER only command buffer
            auto staging = StagingBuffer::Create(m_GraphicsContext, buffer.Size, buffer.Data);

            cmd->Begin();
            cmd->CopyBuffer(staging, static_cast<T*>(this));
            cmd->Flush();
        }
    }

    template <class T>
    BufferAddress VulkanBaseBuffer<T>::CreateAndReturnBufferAddress() const
    {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = m_Buffer;

        return vkGetBufferDeviceAddress(m_GraphicsContext->GetDevice(), &addressInfo);
    }

    /* VulkanBuffer */

    VulkanBuffer::VulkanBuffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : Buffer(context, info), VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    /* VulkanStagingBuffer */

    VulkanStagingBuffer::VulkanStagingBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : VulkanStagingBuffer(context, CreateBufferInfo(size, data)) {}

    VulkanStagingBuffer::VulkanStagingBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : StagingBuffer(context, info), VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    void VulkanStagingBuffer::InitBufferWithData(const SBuffer& buffer)
    {
        EE_PROFILE_ZONE_SCOPED()
        if (buffer.Data)
        {
            void* mapped = Map();
            Memory::Memcpy(mapped, buffer.Data, m_Size);
            Unmap();
        }
    }

    /* VulkanVertexBuffer */

    VulkanVertexBuffer::VulkanVertexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : VulkanVertexBuffer(context, CreateBufferInfo(size, data)) {}

    VulkanVertexBuffer::VulkanVertexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : VertexBuffer(context, info), VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateBufferAddress();
    }

    /* VulkanIndexBuffer */

    VulkanIndexBuffer::VulkanIndexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data,
        const EIndexType type
    ) : VulkanIndexBuffer(context, CreateBufferInfo(size, data), type) {}

    VulkanIndexBuffer::VulkanIndexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        const EIndexType type
    ) : IndexBuffer(context, info, type), VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}