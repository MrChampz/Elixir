#pragma once

#include <Graphics/Vulkan/VulkanImage.h>
#include <Graphics/Vulkan/Converters.h>

namespace Elixir::Vulkan::Initializers
{
    static VmaAllocationCreateInfo AllocationCreateInfo(const SAllocationInfo& info)
    {
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.requiredFlags = Converters::GetMemoryProperties(info.RequiredFlags);
        allocInfo.preferredFlags = Converters::GetMemoryProperties(info.PreferredFlags);

        return allocInfo;
    }

    static VkBufferCreateInfo BufferCreateInfo(const SBufferCreateInfo& info)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;
        bufferInfo.size = (VkDeviceSize)info.Buffer.Size;
        bufferInfo.usage = Converters::GetBufferUsage(info.Usage);

        return bufferInfo;
    }

    static VkSamplerCreateInfo SamplerCreateInfo(const SSamplerCreateInfo& info)
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = Converters::GetSamplerFilter(info.MagFilter);
        samplerInfo.minFilter = Converters::GetSamplerFilter(info.MinFilter);
        samplerInfo.mipmapMode = Converters::GetSamplerMipmapMode(info.MipmapMode);
        samplerInfo.addressModeU = Converters::GetSamplerAddressMode(info.AddressModeU);
        samplerInfo.addressModeV = Converters::GetSamplerAddressMode(info.AddressModeV);
        samplerInfo.addressModeW = Converters::GetSamplerAddressMode(info.AddressModeW);
        samplerInfo.mipLodBias = info.MipLodBias;
        samplerInfo.minLod = info.MinLod;
        samplerInfo.maxLod = info.MaxLod;
        samplerInfo.anisotropyEnable = info.AnisotropyEnabled;
        samplerInfo.maxAnisotropy = info.MaxAnisotropy;
        samplerInfo.compareEnable = info.CompareEnabled;
        samplerInfo.compareOp = Converters::GetCompareOp(info.CompareOp);
        samplerInfo.borderColor = Converters::GetSamplerBorderColor(info.BorderColor);
        samplerInfo.unnormalizedCoordinates = info.UnnormalizedCoordinates;

        return samplerInfo;
    }

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

    static VkImageSubresourceRange ImageSubresourceRange(const EImageAspect aspectMask)
    {
        return ImageSubresourceRange(Converters::GetImageAspect(aspectMask));
    }

    static VkImageCreateInfo ImageCreateInfo(
        const SImageCreateInfo& info,
        const uint32_t queueFamily
    )
    {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = nullptr;
        imageInfo.imageType = Converters::GetImageType(info.Type);
        imageInfo.format = Converters::GetFormat(info.Format);
        imageInfo.extent = { info.Width, info.Height, info.Depth };
        imageInfo.mipLevels = info.MipLevels;
        imageInfo.arrayLayers = info.ArrayLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = Converters::GetImageUsage(info.Usage);
        imageInfo.pQueueFamilyIndices = &queueFamily;
        imageInfo.queueFamilyIndexCount = 1;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        return imageInfo;
    }

    static VkImageViewCreateInfo ImageViewCreateInfo(const Image* image)
    {
        const auto vk_Image = GetVulkanImageHandler(image);
        EE_CORE_ASSERT(vk_Image != nullptr, "Image is invalid!");

        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = nullptr;
        info.viewType = Converters::GetImageViewType(image);
        info.image = vk_Image;
        info.format = Converters::GetFormat(image->GetFormat());
        info.subresourceRange = ImageSubresourceRange(image->GetAspect());

        return info;
    }
}