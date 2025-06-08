#pragma once

#include <Graphics/Vulkan/VulkanImage.h>

namespace Elixir::Vulkan::Initializers
{
    static VkImageSubresourceRange ImageSubresourceRange(const VkImageAspectFlags aspectMask)
    {
        VkImageSubresourceRange range = {};
        range.aspectMask = aspectMask;
        range.baseMipLevel = 0;
        range.levelCount = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;
        range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        return range;
    }

    static VkImageCreateInfo ImageCreateInfo(
        const VkFormat format,
        const VkImageUsageFlags usage,
        const VkExtent3D extent
    )
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.pNext = nullptr;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = extent;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;

        // FOR MSAA
        info.samples = VK_SAMPLE_COUNT_1_BIT;

        return info;
    }

    static VkImageViewCreateInfo ImageViewCreateInfo(
        const VkFormat format,
        const VkImage image,
        const VkImageAspectFlags aspectMask
    )
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = nullptr;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.image = image;
        info.format = format;
        info.subresourceRange = ImageSubresourceRange(aspectMask);

        return info;
    }
}