#include "epch.h"
#include "VulkanTexture.h"

namespace Elixir::Vulkan
{
    /* VulkanTexture */

    VulkanTexture::VulkanTexture(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        void* data,
        const std::string& path
    ) : VulkanTexture(context, CreateImageInfo(format, width, data), path) {}

    VulkanTexture::~VulkanTexture()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanImageBase::Destroy();
    }

    VulkanTexture::VulkanTexture(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : VulkanImageBase(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Path = path;
    }

    /* VulkanTexture2D */

    VulkanTexture2D::VulkanTexture2D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        void* data,
        const std::string& path
    ) : VulkanTexture2D(context, CreateImageInfo(format, width, height, data), path) {}

    VulkanTexture2D::~VulkanTexture2D()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanImageBase::Destroy();
    }

    VulkanTexture2D::VulkanTexture2D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : VulkanImageBase(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Path = path;
    }

    /* VulkanTexture3D */

    VulkanTexture3D::VulkanTexture3D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        void* data,
        const std::string& path
    ) : VulkanTexture3D(context, CreateImageInfo(format, width, height, depth, data), path) {}

    VulkanTexture3D::~VulkanTexture3D()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanImageBase::Destroy();
    }

    VulkanTexture3D::VulkanTexture3D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : VulkanImageBase(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Path = path;

        // TODO: Set a default sampler
        // auto sampler = SamplerBuilder()
        // 	.SetMagFilter(ESamplerFilter::Nearest)
        // 	.SetMinFilter(ESamplerFilter::Nearest)
        // 	.Build();

        // SetSampler(sampler);
    }
}