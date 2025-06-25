#include "epch.h"
#include "Buffer.h"

#include <Engine/Graphics/Utils.h>
#include <Graphics/Vulkan/VulkanBuffer.h>

namespace Elixir
{
    using namespace Elixir::Graphics;

    /* SBufferElement */

    SBufferElement::SBufferElement(
        const EDataType type,
        const std::string& name,
        const bool normalized
    ) : Name(name), Type(type), Offset(0), Size(Utils::GetDataTypeSize(type)),
        Normalized(normalized) {}

    uint32_t SBufferElement::GetComponentCount() const
    {
        return Utils::GetDataTypeComponentCount(Type);
    }

    /* BufferLayout */

    BufferLayout::BufferLayout(const std::initializer_list<SBufferElement>& elements)
        : m_Elements(elements), m_Stride(0)
    {
        CalculateOffsetsAndStride();
    }

    void BufferLayout::CalculateOffsetsAndStride()
    {
        m_Stride = 0;

        size_t offset = 0;
        for (auto& element : m_Elements)
        {
            element.Offset = offset;
            offset += element.Size;
            m_Stride += element.Size;
        }
    }

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
    }

    /* DynamicBuffer */

    DynamicBuffer::DynamicBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : Buffer(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
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

    VertexBuffer::VertexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : VertexBuffer(context, CreateBufferInfo(size, data)) {}

    VertexBuffer::VertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : Buffer(context, info), m_Address(0)
    {
        EE_PROFILE_ZONE_SCOPED()
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

    /* IndexBuffer */

    const auto INDEX_BUFFER_USAGE = EBufferUsage::IndexBuffer |
        EBufferUsage::TransferDst;

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
    ) : Buffer(context, info), m_IndexType(type)
    {
        EE_PROFILE_ZONE_SCOPED()
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
}