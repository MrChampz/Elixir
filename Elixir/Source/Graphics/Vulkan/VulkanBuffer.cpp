#include "epch.h"
#include "VulkanBuffer.h"

#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Graphics/Vulkan/Initializers.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    namespace
    {
        using VulkanBufferTypes = std::tuple<
            VulkanDynamicBuffer<StagingBuffer>,
            VulkanDynamicBuffer<UniformBuffer>,
            VulkanBaseBuffer<VertexBuffer>,
            VulkanBaseBuffer<IndexBuffer>,
            VulkanBaseBuffer<Buffer>
        >;

        template <size_t I = 0>
        VkBuffer TryToGetVulkanBufferHandle(const Buffer* buffer)
        {
            if constexpr (I < std::tuple_size_v<VulkanBufferTypes>)
            {
                using BufferType = std::tuple_element_t<I, VulkanBufferTypes>;

                if (auto* buff = dynamic_cast<const BufferType*>(buffer))
                {
                    return buff->GetVulkanBuffer();
                }

                return TryToGetVulkanBufferHandle<I + 1>(buffer);
            }

            EE_CORE_ASSERT(false, "Unsupported buffer type!")
            return VK_NULL_HANDLE;
        }
    }

    VkBuffer TryToGetVulkanBuffer(const Buffer* buffer)
    {
        if (!buffer) return VK_NULL_HANDLE;
        return TryToGetVulkanBufferHandle(buffer);
    }

    /* VulkanBaseBuffer */

    template <class Base>
    void VulkanBaseBuffer<Base>::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (!this->IsValid()) return;

        vmaDestroyBuffer(m_GraphicsContext->GetAllocator(), m_Buffer, m_Allocation);
        m_Buffer = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;
        m_DescriptorInfo = {};
    }

    template <class Base>
    void VulkanBaseBuffer<Base>::Copy(
        const CommandBuffer* cmd,
        const Buffer* dst,
        const std::span<SBufferCopy> regions
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Dst = TryToGetVulkanBuffer(dst);

        EE_CORE_ASSERT(vk_Dst != VK_NULL_HANDLE, "Invalid destination buffer!")

        std::span copyRegions = regions;

        if (copyRegions.empty())
        {
            const auto& size = this->GetSize();
            SBufferCopy defaultRegion[] = { SBufferCopy::Default(size) };
            copyRegions = std::span(defaultRegion);
        }

        std::vector<VkBufferCopy> bufferCopies(copyRegions.size());

        std::ranges::transform(
            copyRegions,
            bufferCopies.begin(),
            Converters::GetBufferCopy
        );

        vkCmdCopyBuffer(
            vk_Cmd->GetVulkanCommandBuffer(),
            GetVulkanBuffer(),
            vk_Dst,
            bufferCopies.size(),
            bufferCopies.data()
        );
    }

    template <class Base>
    VulkanBaseBuffer<Base>::VulkanBaseBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : Base(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
    }

    template <class Base>
    void VulkanBaseBuffer<Base>::CreateBuffer(const SBufferCreateInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto bufferInfo = Initializers::BufferCreateInfo(info);
        const auto allocInfo = Initializers::AllocationCreateInfo(info.AllocationInfo);

        VK_CHECK_RESULT(
            vmaCreateBuffer(
                m_GraphicsContext->GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer,
                &m_Allocation, nullptr
            )
        );
    }

    template <class Base>
    void VulkanBaseBuffer<Base>::InitBuffer(const SBuffer& buffer)
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
            cmd->CopyBuffer(staging, this);
            cmd->Flush();
        }
    }

    template <class Base>
    void VulkanBaseBuffer<Base>::CreateDescriptorInfo()
    {
        m_DescriptorInfo = {};
        m_DescriptorInfo.buffer = m_Buffer;
        m_DescriptorInfo.offset = 0;
        m_DescriptorInfo.range = VK_WHOLE_SIZE;
    }

    /* VulkanDynamicBuffer */

    template <class Base>
    void* VulkanDynamicBuffer<Base>::Map()
    {
        EE_PROFILE_ZONE_SCOPED()
        void* data;
        VK_CHECK_RESULT(vmaMapMemory(
            this->m_GraphicsContext->GetAllocator(),
            this->m_Allocation,
            &data
        ));
        return data;
    }

    template <class Base>
    void VulkanDynamicBuffer<Base>::Unmap()
    {
        EE_PROFILE_ZONE_SCOPED()
        vmaUnmapMemory(this->m_GraphicsContext->GetAllocator(), this->m_Allocation);
    }

    template <class Base>
    VulkanDynamicBuffer<Base>::VulkanDynamicBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : VulkanBaseBuffer<Base>(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    /* VulkanBuffer */

    VulkanBuffer::VulkanBuffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanBuffer::CreateBuffer(info);
        VulkanBuffer::InitBuffer(info.Buffer);
        VulkanBuffer::CreateDescriptorInfo();
    }

    VulkanBuffer::~VulkanBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanBuffer::Destroy();
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
    )
        : VulkanDynamicBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanStagingBuffer::CreateBuffer(info);
        VulkanStagingBuffer::InitBuffer(info.Buffer);
        VulkanStagingBuffer::CreateDescriptorInfo();
    }

    VulkanStagingBuffer::~VulkanStagingBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanStagingBuffer::Destroy();
    }

    void VulkanStagingBuffer::InitBuffer(const SBuffer& buffer)
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
    )
        : VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanVertexBuffer::CreateBuffer(info);
        VulkanVertexBuffer::InitBuffer(info.Buffer);
        VulkanVertexBuffer::CreateDescriptorInfo();
        CreateBufferAddress();
    }

    VulkanVertexBuffer::~VulkanVertexBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanVertexBuffer::Destroy();
    }

    void VulkanVertexBuffer::CreateBufferAddress()
    {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = m_Buffer;

        m_Address = vkGetBufferDeviceAddress(m_GraphicsContext->GetDevice(), &addressInfo);
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
    )
        : VulkanBaseBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanIndexBuffer::CreateBuffer(info);
        VulkanIndexBuffer::InitBuffer(info.Buffer);
        VulkanIndexBuffer::CreateDescriptorInfo();
    }

    VulkanIndexBuffer::~VulkanIndexBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanIndexBuffer::Destroy();
    }

    /* VulkanUniformBuffer */

    VulkanUniformBuffer::VulkanUniformBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : VulkanUniformBuffer(context, CreateBufferInfo(size, data)) {}

    VulkanUniformBuffer::VulkanUniformBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    )
        : VulkanDynamicBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanUniformBuffer::CreateBuffer(info);
        VulkanUniformBuffer::InitBuffer(info.Buffer);
        VulkanUniformBuffer::CreateDescriptorInfo();
    }

    VulkanUniformBuffer::~VulkanUniformBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanUniformBuffer::Destroy();
    }

    void VulkanUniformBuffer::InitBuffer(const SBuffer& buffer)
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