#include "epch.h"
#include "Buffer.h"

#include "CommandBuffer.h"

#include <Engine/Graphics/Utils.h>
#include <Graphics/Vulkan/VulkanBuffer.h>

namespace Elixir
{
    using namespace Elixir::Graphics;

    /* Buffer */

    void Buffer::Copy(
        const Ref<CommandBuffer>& cmd,
        const Ref<Buffer>& dst,
        const std::span<SBufferCopy> regions
    )
    {
        Copy(cmd.get(), dst.get(), regions);
    }

    void Buffer::Copy(
        const Ref<CommandBuffer>& cmd,
        const Buffer* dst,
        const std::span<SBufferCopy> regions
    )
    {
        Copy(cmd.get(), dst, regions);
    }

    void Buffer::Copy(
        const CommandBuffer* cmd,
        const Ref<Buffer>& dst,
        const std::span<SBufferCopy> regions
    )
    {
        Copy(cmd, dst.get(), regions);
    }

    Ref<Buffer> Buffer::Create(const GraphicsContext* context, const SBufferCreateInfo& info)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanBuffer>(context, info);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Buffer::Buffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : m_Usage(info.Usage), m_Size(info.Buffer.Size), m_AllocationInfo(info.AllocationInfo),
          m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "Buffer[" + m_UUID.ToString() + "]";
#endif
    }

    /* DynamicBuffer */

    DynamicBuffer::DynamicBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : Buffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "DynamicBuffer[" + m_UUID.ToString() + "]";
#endif
    }

    /* StagingBuffer */

    Ref<StagingBuffer> StagingBuffer::Create(
        const GraphicsContext* context,
        size_t size,
        const void* data
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanStagingBuffer>(context, size, data);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SBufferCreateInfo StagingBuffer::CreateBufferInfo(const size_t size, const void* data)
    {
        return {
            .Buffer = SBuffer(data, size),
            .Usage = EBufferUsage::TransferSrc,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent,
                .PreferredFlags = EMemoryProperty::HostCached
            }
        };
    }

    StagingBuffer::StagingBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : StagingBuffer(context, CreateBufferInfo(size, data)) {}

    StagingBuffer::StagingBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : DynamicBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "StagingBuffer[" + m_UUID.ToString() + "]";
#endif
    }

    void VertexBuffer::Bind(
        const Ref<CommandBuffer>& cmd,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    ) const
    {
        const VertexBuffer* buffers[] = { this };
        cmd->BindVertexBuffers(buffers, {}, bindingCount, firstBinding);
    }

    /* VertexBuffer */

    const auto VERTEX_BUFFER_USAGE =
        EBufferUsage::VertexBuffer |
        EBufferUsage::StorageBuffer |
        EBufferUsage::TransferDst |
        EBufferUsage::ShaderDeviceAddress;

    Ref<VertexBuffer> VertexBuffer::Create(
        const GraphicsContext* context,
        size_t size,
        const void* data
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanVertexBuffer>(context, size, data);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SBufferCreateInfo VertexBuffer::CreateBufferInfo(const size_t size, const void* data)
    {
        return {
            .Buffer = SBuffer(data, size),
            .Usage = VERTEX_BUFFER_USAGE,
            .AllocationInfo = {
                .PreferredFlags = EMemoryProperty::DeviceLocal
            }
        };
    }

    VertexBuffer::VertexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : VertexBuffer(context, CreateBufferInfo(size, data)) {}

    VertexBuffer::VertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : Buffer(context, info), m_Address(0)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "VertexBuffer[" + m_UUID.ToString() + "]";
#endif
    }

    /* DynamicVertexBuffer */

    const auto DYNAMIC_VERTEX_BUFFER_USAGE =
        EBufferUsage::VertexBuffer |
        EBufferUsage::StorageBuffer |
        EBufferUsage::ShaderDeviceAddress;

    void DynamicVertexBuffer::Bind(
        const Ref<CommandBuffer>& cmd,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    ) const
    {
        const DynamicVertexBuffer* buffers[] = { this };
        cmd->BindVertexBuffers(buffers, {}, bindingCount, firstBinding);
    }

    void DynamicVertexBuffer::UpdateData(
        const void* data,
        const size_t size,
        const size_t offset
    ) const
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(offset + size <= m_Size, "Buffer overflow!")
        EE_CORE_ASSERT(m_PersistentMapping, "Persistent mapping is required for dynamic buffers!")

        if (m_PersistentMapping)
        {
            Memory::Memcpy((uint8_t*)m_PersistentMapping + offset, data, size);
        }
    }

    Ref<DynamicVertexBuffer> DynamicVertexBuffer::Create(
        const GraphicsContext* context,
        size_t size,
        const void* data
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanDynamicVertexBuffer>(context, size, data);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SBufferCreateInfo DynamicVertexBuffer::CreateBufferInfo(const size_t size, const void* data)
    {
        return {
            .Buffer = SBuffer(data, size),
            .Usage = DYNAMIC_VERTEX_BUFFER_USAGE,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent,
                .PreferredFlags = EMemoryProperty::HostCached
            }
        };
    }

    DynamicVertexBuffer::DynamicVertexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : DynamicVertexBuffer(context, CreateBufferInfo(size, data)) {}

    DynamicVertexBuffer::DynamicVertexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : DynamicBuffer(context, info), m_Address(0)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "DynamicVertexBuffer[" + m_UUID.ToString() + "]";
#endif
    }

    /* IndexBuffer */

    const auto INDEX_BUFFER_USAGE = EBufferUsage::IndexBuffer |
        EBufferUsage::TransferDst;

    void IndexBuffer::Bind(const Ref<CommandBuffer>& cmd) const
    {
        cmd->BindIndexBuffer(this);
    }

    Ref<IndexBuffer> IndexBuffer::Create(
        const GraphicsContext* context,
        size_t size,
        const void* data,
        EIndexType type
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanIndexBuffer>(
                    context,
                    size,
                    data,
                    type
                );
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SBufferCreateInfo IndexBuffer::CreateBufferInfo(const size_t size, const void* data)
    {
        return {
            .Buffer = SBuffer(data, size),
            .Usage = INDEX_BUFFER_USAGE,
            .AllocationInfo = {
                .PreferredFlags = EMemoryProperty::DeviceLocal
            }
        };
    }

    IndexBuffer::IndexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data,
        const EIndexType type
    ) : IndexBuffer(context, CreateBufferInfo(size, data), type) {}

    IndexBuffer::IndexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        const EIndexType type
    )
        : Buffer(context, info), m_IndexType(type)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "IndexBuffer[" + m_UUID.ToString() + "]";
#endif
    }

    /* DynamicIndexBuffer */

    constexpr auto DYNAMIC_INDEX_BUFFER_USAGE = EBufferUsage::IndexBuffer;

    void DynamicIndexBuffer::Bind(const Ref<CommandBuffer>& cmd) const
    {
        cmd->BindIndexBuffer(this);
    }

    void DynamicIndexBuffer::UpdateData(
        const void* data,
        const size_t size,
        const size_t offset
    ) const
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(offset + size <= m_Size, "Buffer overflow!")
        EE_CORE_ASSERT(m_PersistentMapping, "Persistent mapping is required for dynamic buffers!")

        if (m_PersistentMapping)
        {
            Memory::Memcpy((uint8_t*)m_PersistentMapping + offset, data, size);
        }
    }

    Ref<DynamicIndexBuffer> DynamicIndexBuffer::Create(
        const GraphicsContext* context,
        size_t size,
        const void* data,
        EIndexType type
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanDynamicIndexBuffer>(
                    context,
                    size,
                    data,
                    type
                );
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SBufferCreateInfo DynamicIndexBuffer::CreateBufferInfo(const size_t size, const void* data)
    {
        return {
            .Buffer = SBuffer(data, size),
            .Usage = DYNAMIC_INDEX_BUFFER_USAGE,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent,
                .PreferredFlags = EMemoryProperty::HostCached
            }
        };
    }

    DynamicIndexBuffer::DynamicIndexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data,
        const EIndexType type
    ) : DynamicIndexBuffer(context, CreateBufferInfo(size, data), type) {}

    DynamicIndexBuffer::DynamicIndexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        const EIndexType type
    ) : DynamicBuffer(context, info), m_IndexType(type)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "DynamicIndexBuffer[" + m_UUID.ToString() + "]";
#endif
    }

    /* UniformBuffer */

    void UniformBuffer::UpdateData(const void* data, const size_t size, const size_t offset)
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(offset + size <= m_Size, "Buffer overflow!")

        void* mapped = Map();
        Memory::Memcpy((uint8_t*)mapped + offset, data, size);
        Unmap();
    }

    Ref<UniformBuffer> UniformBuffer::Create(
        const GraphicsContext* context,
        size_t size,
        const void* data
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanUniformBuffer>(
                    context,
                    size,
                    data
                );
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SBufferCreateInfo UniformBuffer::CreateBufferInfo(const size_t size, const void* data)
    {
        return {
            .Buffer = SBuffer(data, size),
            .Usage = EBufferUsage::UniformBuffer,
            .AllocationInfo = {
                .PreferredFlags = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent
            }
        };
    }

    UniformBuffer::UniformBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : UniformBuffer(context, CreateBufferInfo(size, data)) {}

    UniformBuffer::UniformBuffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : DynamicBuffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "ConstantBuffer[" + m_UUID.ToString() + "]";
#endif
    }
}