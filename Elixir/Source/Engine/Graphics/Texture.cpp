#include "epch.h"
#include "Texture.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Graphics/Vulkan/VulkanTexture.h>

namespace Elixir
{
    /* Texture */

    Ref<Texture> Texture::Create(
        const GraphicsContext* context,
        const STextureCreateInfo& info
    )
    {
        const auto imageInfo = CreateImageInfo(info);

        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanTexture>(context, imageInfo, info.Path);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Ref<Texture> Texture::Create(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const void* data,
        const std::string& path
    )
    {
        STextureCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Path = path;
        return Create(context, info);
    }

    SImageCreateInfo Texture::CreateImageInfo(
        const STextureCreateInfo& info
    )
    {
        auto usage = EImageUsage::Sampled | EImageUsage::TransferDst;

        if (info.GenerateMipmaps)
            usage |= EImageUsage::TransferSrc;

        return {
            .InitialData = info.InitialData,
            .Width = info.Width,
            .Type = EImageType::_1D,
            .Format = info.Format,
            .MipLevels = info.GenerateMipmaps
                ? GetFullMipLevelCount({ info.Width, 1, 1 })
                : info.MipLevels,
            .Usage = usage,
            .InitialLayout = EImageLayout::ShaderReadOnly,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::DeviceLocal
            },
            .GenerateMipmaps = info.GenerateMipmaps,
        };
    }

    SImageCreateInfo Texture::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const void* data,
        const std::string& path
    )
    {
        STextureCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Path = path;
        return CreateImageInfo(info);
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
        const STexture2DCreateInfo& info
    )
    {
        const auto imageInfo = CreateImageInfo(info);

        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanTexture2D>(context, imageInfo, info.Path);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Ref<Texture2D> Texture2D::Create(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const void* data,
        const std::string& path
    )
    {
        STexture2DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        info.Path = path;
        return Create(context, info);
    }

    SImageCreateInfo Texture2D::CreateImageInfo(
        const STexture2DCreateInfo& info
    )
    {

        auto usage = EImageUsage::Sampled | EImageUsage::TransferDst;

        if (info.GenerateMipmaps)
            usage |= EImageUsage::TransferSrc;

        return {
            .InitialData = info.InitialData,
            .Width = info.Width,
            .Height = info.Height,
            .Type = EImageType::_2D,
            .Format = info.Format,
            .MipLevels = info.GenerateMipmaps
                ? GetFullMipLevelCount({ info.Width, info.Height, 1 })
                : info.MipLevels,
            .Usage = usage,
            .InitialLayout = EImageLayout::ShaderReadOnly,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::DeviceLocal
            },
            .GenerateMipmaps = info.GenerateMipmaps,
        };
    }

    SImageCreateInfo Texture2D::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const void* data
    )
    {
        STexture2DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        return CreateImageInfo(info);
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
        const STexture3DCreateInfo& info
    )
    {
        const auto imageInfo = CreateImageInfo(info);

        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanTexture3D>(context, imageInfo, info.Path);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Ref<Texture3D> Texture3D::Create(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        const void* data,
        const std::string& path
    )
    {
        STexture3DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        info.Depth = depth;
        info.Path = path;
        return Create(context, info);
    }

    SImageCreateInfo Texture3D::CreateImageInfo(
        const STexture3DCreateInfo& info
    )
    {
        auto usage = EImageUsage::Sampled | EImageUsage::TransferDst;

        if (info.GenerateMipmaps)
            usage |= EImageUsage::TransferSrc;

        return {
            .InitialData = info.InitialData,
            .Width = info.Width,
            .Height = info.Height,
            .Depth = info.Depth,
            .Type = EImageType::_3D,
            .Format = info.Format,
            .MipLevels = info.GenerateMipmaps
                ? GetFullMipLevelCount({ info.Width, info.Height, info.Depth })
                : info.MipLevels,
            .Usage = usage,
            .InitialLayout = EImageLayout::ShaderReadOnly,
            .AllocationInfo = {
                .RequiredFlags = EMemoryProperty::DeviceLocal
            },
            .GenerateMipmaps = info.GenerateMipmaps,
        };
    }

    SImageCreateInfo Texture3D::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        const void* data
    )
    {
        STexture3DCreateInfo info;
        info.InitialData = data;
        info.Format = format;
        info.Width = width;
        info.Height = height;
        info.Depth = depth;
        return CreateImageInfo(info);
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
