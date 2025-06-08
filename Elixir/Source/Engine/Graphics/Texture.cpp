#include "epch.h"
#include "Texture.h"

#include <utility>

#include "GraphicsContext.h"

namespace Elixir
{
    /* Texture */

    Ref<Texture> Texture::Create(
        const GraphicsContext* context,
        EImageFormat format,
        uint32_t width,
        void* data,
        const std::string& path
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

    Texture::Texture(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        void* data,
        std::string path
    ) : Image(context, format, width, data), m_Path(std::move(path))
    {
        m_Type = EImageType::_1D;
    }

    /* Texture2D */

    Ref<Texture2D> Texture2D::Create(
        const GraphicsContext* context,
        EImageFormat format,
        uint32_t width,
        uint32_t height,
        void* data,
        const std::string& path
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

    Texture2D::Texture2D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        void* data,
        const std::string& path
    ) : Texture(context, format, width, data, path), m_Height(height)
    {
        m_Type = EImageType::_2D;
    }

    /* Texture3D */

    Ref<Texture3D> Texture3D::Create(
        const GraphicsContext* context,
        EImageFormat format,
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        void* data,
        const std::string& path
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

    Texture3D::Texture3D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        void* data,
        const std::string& path
    ) : Texture2D(context, format, width, height, data, path), m_Depth(depth)
    {
        m_Type = EImageType::_3D;
    }
}