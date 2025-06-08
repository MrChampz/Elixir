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
    ) : Image(context, format, width, data),
        Texture(context, format, width, data, path),
        VulkanBaseImage(context, format, width, data)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanTexture::CreateImage(data);

        // TODO: Set a default sampler
		// auto sampler = SamplerBuilder()
		// 	.SetMagFilter(ESamplerFilter::Nearest)
		// 	.SetMinFilter(ESamplerFilter::Nearest)
		// 	.Build();

        //SetSampler(sampler);
    }

    /* VulkanTexture2D */

    VulkanTexture2D::VulkanTexture2D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        void* data,
        const std::string& path
    ) : Image(context, format, width, data),
        Texture2D(context, format, width, height, data, path),
        VulkanBaseImage(context, format, width, data)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanTexture2D::CreateImage(data);

        // TODO: Set a default sampler
        // auto sampler = SamplerBuilder()
        // 	.SetMagFilter(ESamplerFilter::Nearest)
        // 	.SetMinFilter(ESamplerFilter::Nearest)
        // 	.Build();

        //SetSampler(sampler);
    }

    VulkanTexture2D::VulkanTexture2D(
        const GraphicsContext* context,
        EImageFormat format,
        VkImage image,
        VkImageView imageView,
        VkExtent2D extent,
        EImageUsage usage
    ) : Image(context, format, extent.width),
        Texture2D(context, format, extent.width, extent.height),
        VulkanBaseImage(context, format, extent.width)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Image = image;
        m_ImageView = imageView;
        m_Usage = usage;
        m_IsSwapchainImage = true;
    }

    VulkanTexture2D::~VulkanTexture2D()
    {
        if (!m_Destroyed)
        {
            vkDestroyImageView(m_GraphicsContext->GetDevice(), m_ImageView, nullptr);

            if (!m_IsSwapchainImage)
                vmaDestroyImage(m_GraphicsContext->GetAllocator(), m_Image, m_Allocation);

            m_Destroyed = true;
        }
    }

    Ref<VulkanTexture2D> VulkanTexture2D::FromSwapchain(
        const GraphicsContext* context,
        EImageFormat format,
        VkImage image,
        VkImageView imageView,
        VkExtent2D extent,
        EImageUsage usage
    )
    {
        return CreateRef<VulkanTexture2D>(context, format, image, imageView, extent, usage);
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
    ):  Image(context, format, width, data),
        Texture3D(context, format, width, height, depth, data, path),
        VulkanBaseImage(context, format, width, data)
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanTexture3D::CreateImage(data);

        // TODO: Set a default sampler
        // auto sampler = SamplerBuilder()
        // 	.SetMagFilter(ESamplerFilter::Nearest)
        // 	.SetMinFilter(ESamplerFilter::Nearest)
        // 	.Build();

        //SetSampler(sampler);
    }
}