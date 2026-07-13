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
        const void* data,
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

    SImageCreateInfo Texture::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const void* data,
        const std::string& path
    )
    {
        return {
            .InitialData = data,
            .Width = width,
            .Type = EImageType::_1D,
            .Format = format,
            .Usage = EImageUsage::Sampled | EImageUsage::TransferDst,
            .InitialLayout = EImageLayout::ShaderReadOnly,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::DeviceLocal
            }
        };
    }

    Texture::Texture(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const void* data,
        const std::string& path
    ) : Texture(context, CreateImageInfo(format, width, data), path) {}

    Texture::Texture(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : Image(context, info), m_Path(std::move(path))
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "Texture[" + m_UUID.ToString() + "]";
#endif
    }

    /* Texture2D */

    Ref<Texture2D> Texture2D::Create(
        const GraphicsContext* context,
        EImageFormat format,
        uint32_t width,
        uint32_t height,
        const void* data,
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
        const void* data
    )
    {
        // Generate a full mip chain for uploaded textures so minified sampling
        // (tiled detail/normal maps) is filtered instead of aliasing, and so the
        // HDR environment can be roughness-prefiltered by sampling coarser mips.
        // Restricted to formats that support a linear blit on this device.
        const bool wantMips = data != nullptr
            && (format == EImageFormat::R8G8B8A8_SRGB
                || format == EImageFormat::R8G8B8A8_UNORM
                || format == EImageFormat::R32G32B32A32_SFLOAT);
        const uint32_t mipLevels = wantMips
            ? (uint32_t)std::floor(std::log2((float)std::max(width, height))) + 1u
            : 1u;

        auto usage = EImageUsage::Sampled | EImageUsage::TransferDst;
        if (wantMips)
            usage |= EImageUsage::TransferSrc;

        return {
            .InitialData = data,
            .Width = width,
            .Height = height,
            .Type = EImageType::_2D,
            .Format = format,
            .MipLevels = mipLevels,
            .Usage = usage,
            .InitialLayout = EImageLayout::ShaderReadOnly,
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
        const void* data,
        const std::string& path
    ) : Texture2D(context, CreateImageInfo(format, width, height, data), path) {}

    Texture2D::Texture2D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : Texture(context, info, path)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "Texture2D[" + m_UUID.ToString() + "]";
#endif
    }

    /* Texture3D */

    Ref<Texture3D> Texture3D::Create(
        const GraphicsContext* context,
        EImageFormat format,
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        const void* data,
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
        const void* data
    )
    {
        return {
            .InitialData = data,
            .Width = width,
            .Height = height,
            .Depth = depth,
            .Type = EImageType::_3D,
            .Format = format,
            .Usage = EImageUsage::Sampled | EImageUsage::TransferDst,
            .InitialLayout = EImageLayout::ShaderReadOnly,
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
        const void* data,
        const std::string& path
    ) : Texture3D(context, CreateImageInfo(format, width, height, depth, data), path) {}

    Texture3D::Texture3D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : Texture2D(context, info, path)
    {
        EE_PROFILE_ZONE_SCOPED()

#ifdef EE_DEBUG
        m_DebugName = "Texture3D[" + m_UUID.ToString() + "]";
#endif
    }
}