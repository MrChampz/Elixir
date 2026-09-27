#include "epch.h"
#include "VulkanImage.h"

#include <Graphics/Vulkan/VulkanBuffer.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    const VulkanImage* TryToGetVulkanImage(const Image* image)
    {
        return dynamic_cast<const VulkanImage*>(image);
    }

    VkImage TryToGetVulkanImageHandle(const Image* image)
    {
        const auto* vkImage = TryToGetVulkanImage(image);
        return vkImage ? vkImage->GetVulkanImage() : VK_NULL_HANDLE;
    }

    VulkanImage::VulkanImage(const GraphicsContext* context, const SImageCreateInfo& info)
      : Image(context, info),
        m_Context((const VulkanGraphicsContext*)context) {}

    VulkanImage::~VulkanImage()
    {
        Destroy();
    }

    void VulkanImage::Destroy()
    {
        if (m_ImageView)
            vkDestroyImageView(m_Context->GetDevice(), m_ImageView, nullptr);

        if (m_Image)
            vmaDestroyImage(m_Context->GetAllocator(), m_Image, m_Allocation);

        m_Image = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;
        m_DescriptorInfo = {};
    }

    void VulkanImage::CreateResource(const SImageCreateInfo& info)
    {
        if (info.MipmapMode == EImageMipmapMode::SimpleAverage && info.MipLevels > 1)
        {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(
                m_Context->GetGPU(),
                Converters::GetFormat(info.Format),
                &properties
            );

            constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                VK_FORMAT_FEATURE_BLIT_DST_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

            if (m_Aspect != EImageAspect::Color ||
               (properties.optimalTilingFeatures & required) != required)
            {
                EE_CORE_ERROR("The Vulkan format does not support linear mip generation.")
                return;
            }
        }

        const uint32_t queueFamily = m_Context->GetGraphicsQueueFamily();
        const auto imageInfo = Initializers::ImageCreateInfo(info, queueFamily);
        const auto allocInfo = Initializers::AllocationCreateInfo(info.AllocationInfo);
        VK_CHECK_RESULT(vmaCreateImage(
            m_Context->GetAllocator(), &imageInfo, &allocInfo, &m_Image, &m_Allocation, nullptr
        ));

        if (!m_DebugName.empty())
            vmaSetAllocationName(m_Context->GetAllocator(), m_Allocation, m_DebugName.c_str());

        constexpr auto viewUsage = EImageUsage::Sampled |
            EImageUsage::Storage |
            EImageUsage::ColorAttachment |
            EImageUsage::DepthStencilAttachment |
            EImageUsage::InputAttachment;

        if (HasAnyFlags(m_Usage, viewUsage))
        {
            const auto viewInfo = Initializers::ImageViewCreateInfo(this);
            VK_CHECK_RESULT(
                vkCreateImageView(
                    m_Context->GetDevice(),
                    &viewInfo,
                    nullptr,
                    &m_ImageView
                )
            );
        }

        m_DescriptorInfo = {};
        m_DescriptorInfo.imageLayout = Converters::GetImageLayout(m_Layout);
        m_DescriptorInfo.imageView = m_ImageView;
    }

    void VulkanImage::Transition(const CommandBuffer* cmd, const EImageLayout layout)
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("Cannot transition an image without native storage.")
            return;
        }

        if (layout == m_Layout)
            return;

        if (layout == EImageLayout::Undefined || layout == EImageLayout::PreInitialized)
        {
            EE_CORE_ERROR("Cannot transition an image into an initial-only layout.")
            return;
        }

        const auto* vkCmd = static_cast<const VulkanCommandBuffer*>(cmd);
        CommandUtils::TransitionImage(
            vkCmd->GetVulkanCommandBuffer(),
            m_Image,
            Converters::GetImageLayout(m_Layout),
            Converters::GetImageLayout(layout),
            Converters::GetImageAspect(m_Aspect)
        );

        m_Layout = layout;
        m_DescriptorInfo.imageLayout = Converters::GetImageLayout(layout);
    }

    void VulkanImage::CopyMip(
        const CommandBuffer* cmd, Image* dst,
        const Extent3D& srcExtent, const Extent3D& dstExtent,
        const uint32_t level
    )
    {
        const auto* vkCmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto target = TryToGetVulkanImageHandle(dst);

        if (!IsValid() || !target || dst->GetArrayLayers() != m_ArrayLayers ||
            dst->GetAspect() != m_Aspect ||
            level >= m_MipLevels || level >= dst->GetMipLevels())
        {
            EE_CORE_ERROR("Invalid source image or incompatible image blit destination.")
            return;
        }

        const bool depthStencil = (m_Aspect & EImageAspect::Depth) ||
            (m_Aspect & EImageAspect::Stencil);

        for (const auto aspect : { EImageAspect::Color, EImageAspect::Depth, EImageAspect::Stencil })
        {
            if (!(m_Aspect & aspect))
                continue;

            CommandUtils::CopyImageToImage(
                vkCmd->GetVulkanCommandBuffer(),
                m_Image,
                target,
                Converters::GetExtent3D(srcExtent),
                Converters::GetExtent3D(dstExtent),
                Converters::GetImageAspect(aspect),
                depthStencil ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
                level,
                level,
                m_ArrayLayers
            );
        }
    }

    void VulkanImage::CopyFrom(
        const CommandBuffer* cmd, const Buffer* src,
        const std::span<SBufferImageCopy> regions
    )
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("Cannot upload to an image without native storage.")
            return;
        }

        const auto* vkCmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto source = TryToGetVulkanBuffer(src);
        EE_CORE_ASSERT(source != VK_NULL_HANDLE, "Invalid source buffer!")

        SBufferImageCopy defaultRegion{};
        defaultRegion.ImageSubresource = { m_Aspect, 0, 0, m_ArrayLayers };
        defaultRegion.ImageExtent = m_Extent;

        const auto copyRegions = regions.empty()
            ? std::span(&defaultRegion, 1)
            : regions;
        std::vector<VkBufferImageCopy> copies(copyRegions.size());
        std::ranges::transform(
            copyRegions,
            copies.begin(),
            Converters::GetBufferImageCopy
        );

        vkCmdCopyBufferToImage(
            vkCmd->GetVulkanCommandBuffer(),
            source,
            m_Image,
            Converters::GetImageLayout(m_Layout),
            uint32_t(copies.size()),
            copies.data()
        );
    }

    void VulkanImage::GenerateMipmaps(const CommandBuffer* cmd, const EImageLayout finalLayout)
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("Cannot generate mipmaps for an image without native storage.")
            return;
        }

        const auto vkCmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto aspect = Converters::GetImageAspect(m_Aspect);
        const auto targetLayout = Converters::GetImageLayout(finalLayout);

        for (uint32_t level = 1; level < m_MipLevels; ++level)
        {
            CommandUtils::TransitionImage(
                vkCmd->GetVulkanCommandBuffer(),
                m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                aspect,
                level - 1,
                1
            );

            CommandUtils::CopyImageToImage(
                vkCmd->GetVulkanCommandBuffer(),
                m_Image,
                m_Image,
                Converters::GetExtent3D(GetMipExtent(level - 1)),
                Converters::GetExtent3D(GetMipExtent(level)),
                aspect,
                VK_FILTER_LINEAR,
                level - 1,
                level,
                m_ArrayLayers
            );

            CommandUtils::TransitionImage(
                vkCmd->GetVulkanCommandBuffer(),
                m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                targetLayout,
                aspect,
                level - 1,
                1
            );
        }

        CommandUtils::TransitionImage(
            vkCmd->GetVulkanCommandBuffer(),
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            targetLayout,
            aspect,
            m_MipLevels - 1,
            1
        );

        m_Layout = finalLayout;
        m_DescriptorInfo.imageLayout = targetLayout;
    }
}
