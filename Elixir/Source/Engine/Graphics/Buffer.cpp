#include "epch.h"
#include "Buffer.h"

#include <Engine/Graphics/Utils.h>

namespace Elixir
{
    using namespace Elixir::Graphics;

    /* SBufferElement */

    SBufferElement::SBufferElement(
        const EDataType type,
        const std::string& name,
        const bool normalized
    ) : Name(name), Type(type), Size(Utils::GetDataTypeSize(type)), Offset(0),
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

    /* GraphicsBuffer */

    GraphicsBuffer::GraphicsBuffer(const GraphicsContext* context, size_t size)
        : m_GraphicsContext(context), m_Size(size)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    /* VertexBuffer */

    Ref<VertexBuffer> VertexBuffer::Create(const GraphicsContext* context, size_t size)
    {
        return Create(context, nullptr, size);
    }

    Ref<VertexBuffer> VertexBuffer::Create(
        const GraphicsContext* context,
        const void* data,
        size_t size
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return nullptr;
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    VertexBuffer::VertexBuffer(const GraphicsContext* context, size_t size)
        : GraphicsBuffer(context, size)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    /* IndexBuffer */

    Ref<IndexBuffer> IndexBuffer::Create(
        const GraphicsContext* context,
        const void* data,
        size_t size,
        EIndexType type
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return nullptr;
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    IndexBuffer::IndexBuffer(
        const GraphicsContext* context,
        size_t size,
        EIndexType type
    ) : GraphicsBuffer(context, size), m_IndexType(type)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}