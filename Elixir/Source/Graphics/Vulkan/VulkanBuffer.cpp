#include "epch.h"
#include "VulkanBuffer.h"

#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Graphics/Vulkan/Initializers.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    /* VulkanBaseBuffer */

    void VulkanBaseBuffer::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_Destroyed) return;

        vmaDestroyBuffer(m_GraphicsContext->GetAllocator(), m_Buffer, m_Allocation);
        m_Buffer = VK_NULL_HANDLE;
        m_Destroyed = true;
    }

    void VulkanBaseBuffer::Copy(
        const CommandBuffer* cmd,
        const Buffer* dst,
        const std::span<SBufferCopy> regions
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Dst = dynamic_cast<const VulkanBaseBuffer*>(dst);

        EE_CORE_ASSERT(vk_Dst != nullptr, "Invalid destination buffer!")
        EE_CORE_ASSERT(vk_Dst->GetVulkanBuffer() != VK_NULL_HANDLE, "Invalid destination buffer!")

        std::span copyRegions = regions;

        if (copyRegions.empty())
        {
            const auto& size = dynamic_cast<const Buffer*>(this)->GetSize();
            SBufferCopy defaultRegion[] = {{ .Size = size }};
            copyRegions = std::span(defaultRegion);
        }

        vkCmdCopyBuffer(
            vk_Cmd->GetVulkanCommandBuffer(),
            GetVulkanBuffer(),
            vk_Dst->GetVulkanBuffer(),
            copyRegions.size(),
            (const VkBufferCopy*)copyRegions.data()
        );
    }

    VulkanBaseBuffer::VulkanBaseBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : m_Buffer(VK_NULL_HANDLE), m_Allocation(nullptr), m_AllocationInfo{}, m_DescriptorInfo{},
        m_Destroyed(false)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
    }

    void VulkanBaseBuffer::CreateBuffer(const SBufferCreateInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto bufferInfo = Initializers::BufferCreateInfo(info);
        const auto allocInfo = Initializers::AllocationCreateInfo(info.AllocationInfo);

        VK_CHECK_RESULT(
            vmaCreateBuffer(
                m_GraphicsContext->GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer,
                &m_Allocation, &m_AllocationInfo
            )
        );

        m_DescriptorInfo = {};
        m_DescriptorInfo.buffer = m_Buffer;
        m_DescriptorInfo.offset = 0;
        m_DescriptorInfo.range = VK_WHOLE_SIZE;
    }

    void VulkanBaseBuffer::InitBufferWithData(const SBuffer& buffer)
    {
        if (buffer.Data)
        {
            auto& cmd = m_GraphicsContext->GetCommandBuffer();   // TODO: Should have a TRANSFER only command buffer
            const auto staging = StagingBuffer::Create(
                m_GraphicsContext,
                buffer.Size,
                buffer.Data
            );

            cmd->Begin();
            cmd->CopyBuffer(staging, dynamic_cast<const Buffer*>(this));
            cmd->Flush();
        }
    }

    BufferAddress VulkanBaseBuffer::CreateAndReturnBufferAddress() const
    {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = m_Buffer;

        return vkGetBufferDeviceAddress(m_GraphicsContext->GetDevice(), &addressInfo);
    }

    /* VulkanDynamicBuffer */

    void* VulkanDynamicBuffer::Map()
    {
        EE_PROFILE_ZONE_SCOPED()
        void* data;
        VK_CHECK_RESULT(vmaMapMemory(m_GraphicsContext->GetAllocator(), m_Allocation, &data));
        return data;
    }

    void VulkanDynamicBuffer::Unmap()
    {
        EE_PROFILE_ZONE_SCOPED()
        vmaUnmapMemory(m_GraphicsContext->GetAllocator(), m_Allocation);
    }

    VulkanDynamicBuffer::VulkanDynamicBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    /* VulkanBuffer */

    VulkanBuffer::VulkanBuffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : Buffer(context, info), VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateBuffer(info);
        InitBufferWithData(info.Buffer);
    }

    VulkanBuffer::~VulkanBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
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
    ) : StagingBuffer(context, info), VulkanDynamicBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateBuffer(info);
        InitBufferWithData(info.Buffer);
    }

    VulkanStagingBuffer::~VulkanStagingBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
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
        CreateBuffer(info);
        InitBufferWithData(info.Buffer);
        CreateBufferAddress();
    }

    VulkanVertexBuffer::~VulkanVertexBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
    }

    void VulkanVertexBuffer::CreateBufferAddress()
    {
        m_Address = CreateAndReturnBufferAddress();
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
        CreateBuffer(info);
        InitBufferWithData(info.Buffer);
    }

    VulkanIndexBuffer::~VulkanIndexBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
    }

    /* VulkanUniformBuffer */

    VulkanUniformBuffer::VulkanUniformBuffer(
        const GraphicsContext* context,
        size_t size,
        const void* data
    ) : VulkanUniformBuffer(context, CreateBufferInfo(size, data)) {}

    VulkanUniformBuffer::VulkanUniformBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : UniformBuffer(context, info), VulkanDynamicBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateBuffer(info);
        InitBufferWithData(info.Buffer);
    }

    VulkanUniformBuffer::~VulkanUniformBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
    }

    void VulkanUniformBuffer::InitBufferWithData(const SBuffer& buffer)
    {
        EE_PROFILE_ZONE_SCOPED()
        if (buffer.Data)
        {
            void* mapped = Map();
            Memory::Memcpy(mapped, buffer.Data, m_Size);
            Unmap();
        }
    }
}