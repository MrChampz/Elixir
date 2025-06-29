#include "epch.h"
#include "Texture.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Graphics/Vulkan/VulkanTexture.h>

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
                return CreateRef<Vulkan::VulkanTexture>(context, format, width, data, path);
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
        const std::string& path
    ) : Texture(context, CreateImageInfo(format, width, data), path) {}

    Texture::Texture(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : Image(context, info), m_Path(std::move(path))
    {
        EE_PROFILE_ZONE_SCOPED()
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
                return CreateRef<Vulkan::VulkanTexture2D>(context, format, width, height, data, path);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SImageCreateInfo Texture2D::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        void* data
    )
    {
        return {
            .InitialData = data,
            .Width = width,
            .Height = height,
            .Type = EImageType::_2D,
            .Format = format,
            .Usage = EImageUsage::Sampled,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::DeviceLocal
            }
        };
    }

    Texture2D::Texture2D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        void* data,
        const std::string& path
    ) : Texture2D(context, CreateImageInfo(format, width, height, data), path) {}

    Texture2D::Texture2D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : Texture(context, info, path)
    {
        EE_PROFILE_ZONE_SCOPED()
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
                return CreateRef<Vulkan::VulkanTexture3D>(context, format, width, height, depth, data, path);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    SImageCreateInfo Texture3D::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        void* data
    )
    {
        return {
            .InitialData = data,
            .Width = width,
            .Height = height,
            .Depth = depth,
            .Type = EImageType::_3D,
            .Format = format,
            .Usage = EImageUsage::Sampled,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::DeviceLocal
            }
        };
    }

    Texture3D::Texture3D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        void* data,
        const std::string& path
    ) : Texture3D(context, CreateImageInfo(format, width, height, depth, data), path) {}

    Texture3D::Texture3D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : Texture2D(context, info, path)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}