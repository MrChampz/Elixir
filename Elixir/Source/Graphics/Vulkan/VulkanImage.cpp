#include "epch.h"
#include "VulkanImage.h"

#include <Engine/Graphics/Converters.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Graphics/Vulkan/Converters.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    using namespace Elixir::Graphics::Converters;
    using namespace Elixir::Graphics::Utils;
    using namespace Elixir::Vulkan;

    /* VulkanBaseImage */

    VulkanBaseImage::~VulkanBaseImage()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (!m_Destroyed)
        {
            vkDestroyImageView(m_GraphicsContext->GetDevice(), m_ImageView, nullptr);
            vmaDestroyImage(m_GraphicsContext->GetAllocator(), m_Image, m_Allocation);
            m_Destroyed = true;
        }
    }

    void VulkanBaseImage::Transition(const CommandBuffer* cmd, const EImageLayout layout)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);

        CommandUtils::TransitionImage(
            vk_Cmd->GetVulkanCommandBuffer(),
            m_Image,
            Converters::GetImageLayout(m_Layout),
            Converters::GetImageLayout(layout),
            Converters::GetImageAspect(m_Aspect)
        );

        m_Layout = layout;
    }

    void VulkanBaseImage::Copy(
        const CommandBuffer* cmd,
        const Ref<Image>& dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Dst = dynamic_pointer_cast<VulkanBaseImage>(dst);

        CommandUtils::CopyImageToImage(
            vk_Cmd->GetVulkanCommandBuffer(),
            m_Image,
            vk_Dst->GetImage(),
            Converters::GetExtent3D(srcExtent),
            Converters::GetExtent3D(dstExtent)
        );
    }

    VkFormat VulkanBaseImage::GetVulkanFormat() const
    {
        return Converters::GetFormat(m_Format);
    }

    VkExtent3D VulkanBaseImage::GetVulkanExtent() const
    {
        return Converters::GetExtent3D(GetExtent());
    }

    VulkanBaseImage::VulkanBaseImage(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        void* data
    ) : Image(context, format, width, data)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
    }

    void VulkanBaseImage::CreateImage(void* data)
    {
        EE_PROFILE_ZONE_SCOPED()

        auto& cmd = m_GraphicsContext->GetCommandBuffer(); // TODO: Should have a TRANSFER only command buffer
        auto vk_Cmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

        const auto device = m_GraphicsContext->GetDevice();
        const uint32_t queueFamily = m_GraphicsContext->GetGraphicsQueueFamily();

        const auto type = Converters::GetImageType(m_Type);
        const auto format = Converters::GetFormat(m_Format);
        const auto usage = Converters::GetImageUsage(m_Usage);
        const auto layout = Converters::GetImageLayout(m_Layout);
        const auto extent = GetVulkanExtent();
        constexpr uint32_t layers = 1;

        VkImageCreateInfo imageInfo = Initializers::ImageCreateInfo(format, usage, extent);
        imageInfo.pQueueFamilyIndices = &queueFamily;
        imageInfo.queueFamilyIndexCount = 1;
        imageInfo.imageType = type;
        imageInfo.initialLayout = layout;
        //imageInfo.mipLevels
        imageInfo.arrayLayers = layers;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VK_CHECK_RESULT(
            vmaCreateImage(
                m_GraphicsContext->GetAllocator(),
                &imageInfo,
                &allocInfo,
                &m_Image,
                &m_Allocation,
                nullptr
            )
        );

        cmd->Begin();

        if (data)
        {
            Transition(cmd.get(), EImageLayout::TransferDst);

            // auto stagingBuffer = VulkanBaseBuffer::CreateBuffer(
            //     m_Size,
            //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            //     VMA_MEMORY_USAGE_CPU_TO_GPU
            // );
            //
            // void* mapped = stagingBuffer->Map();
            // memcpy(mapped, data, m_Size);
            //
            // BufferImageCopy copyRegion = {};
            // copyRegion.ImageSubresource.AspectMask = aspect;
            // copyRegion.ImageSubresource.MipLevel = 0;
            // copyRegion.ImageSubresource.BaseArrayLayer = 0;
            // copyRegion.ImageSubresource.LayerCount = layers;
            // copyRegion.ImageExtent = GetExtent();
            //
            // std::array<BufferImageCopy, 1> regions{ copyRegion };
            // CopyFromBuffer(cmd, stagingBuffer, regions);

            Transition(cmd.get(), EImageLayout::ShaderReadOnly);
            cmd->Flush();

            // stagingBuffer->Destroy();
        }
        else
        {
            Transition(cmd.get(), EImageLayout::ShaderReadOnly);
            cmd->Flush();
        }

        auto viewInfo = Initializers::ImageViewCreateInfo(
            format,
            m_Image,
            Converters::GetImageAspect(m_Aspect)
        );
        viewInfo.viewType = Converters::GetImageViewType(m_Type, layers);
        VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView));

        m_DescriptorInfo = VkDescriptorImageInfo{};
        m_DescriptorInfo.imageLayout = Converters::GetImageLayout(m_Layout);
        m_DescriptorInfo.imageView = m_ImageView;
    }

    void VulkanBaseImage::UpdateSampler()
    {
        EE_PROFILE_ZONE_SCOPED()
        // auto sampler = std::static_pointer_cast<VulkanSampler>(m_Sampler);
        // m_DescriptorInfo.sampler = sampler->GetVulkanSampler();
    }

    /* VulkanImage */

    VulkanImage::VulkanImage(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        void* data
    ) : Image(context, format, width, data), VulkanBaseImage(context, format, width, data)
    {
        EE_PROFILE_ZONE_SCOPED()

        VulkanBaseImage::CreateImage(data);

        // TODO: Set a default sampler
        // auto sampler = SamplerBuilder()
        //     .SetMagFilter(ESamplerFilter::Nearest)
        //     .SetMinFilter(ESamplerFilter::Nearest)
        //     .Build();

        // SetSampler(sampler);
    }

    /* VulkanDepthStencilImage */

    VulkanDepthStencilImage::VulkanDepthStencilImage(
        const GraphicsContext* context,
        const EDepthStencilImageFormat format,
        const uint32_t width,
        const uint32_t height
    ) : Image(context, GetImageFormat(format), width),
        DepthStencilImage(context, format, width, height),
        VulkanBaseImage(context, GetImageFormat(format), width)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateImage(nullptr);
    }

    void VulkanDepthStencilImage::CreateImage(void* data)
    {
        EE_PROFILE_ZONE_SCOPED()

        auto& cmd = m_GraphicsContext->GetCommandBuffer(); // TODO: Should have a TRANSFER only command buffer
        auto vk_Cmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

        const auto device = m_GraphicsContext->GetDevice();
        const uint32_t queueFamily = m_GraphicsContext->GetGraphicsQueueFamily();

        const auto type = Converters::GetImageType(m_Type);
        const auto format = Converters::GetFormat(m_Format);
        const auto usage = Converters::GetImageUsage(m_Usage);
        const auto layout = Converters::GetImageLayout(EImageLayout::Undefined);
        const auto extent = GetVulkanExtent();

        VkImageCreateInfo imageInfo = Initializers::ImageCreateInfo(format, usage, extent);
        imageInfo.pQueueFamilyIndices = &queueFamily;
        imageInfo.queueFamilyIndexCount = 1;
        imageInfo.imageType = type;
        imageInfo.initialLayout = layout;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VK_CHECK_RESULT(
            vmaCreateImage(
                m_GraphicsContext->GetAllocator(),
                &imageInfo,
                &allocInfo,
                &m_Image,
                &m_Allocation,
                nullptr
            )
        );

        cmd->Begin();
        Transition(cmd.get(), EImageLayout::DepthAttachment);
        cmd->Flush();

        const auto viewInfo = Initializers::ImageViewCreateInfo(
            format,
            m_Image,
            Converters::GetImageAspect(m_Aspect)
        );
        VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView));
    }

    /* VulkanStorageImage */

    VulkanStorageImage::VulkanStorageImage(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height
    ) : Image(context, format, width),
        StorageImage(context, format, width, height),
        VulkanBaseImage(context, format, width)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateImage(nullptr);

        // TODO: Set a default sampler
        // auto sampler = SamplerBuilder()
        //     .SetMagFilter(ESamplerFilter::Nearest)
        //     .SetMinFilter(ESamplerFilter::Nearest)
        //     .Build();

        // SetSampler(sampler);
    }

    void VulkanStorageImage::CreateImage(void* data)
    {
        EE_PROFILE_ZONE_SCOPED()

        auto& cmd = m_GraphicsContext->GetCommandBuffer(); // TODO: Should have a TRANSFER only command buffer
        auto vk_Cmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

        const auto device = m_GraphicsContext->GetDevice();
        const uint32_t queueFamily = m_GraphicsContext->GetGraphicsQueueFamily();

        const auto type = Converters::GetImageType(m_Type);
        const auto format = GetVulkanFormat();
        const auto usage = Converters::GetImageUsage(m_Usage);
        const auto layout = Converters::GetImageLayout(m_Layout);
        const auto extent = GetVulkanExtent();

        VkImageCreateInfo imageInfo = Initializers::ImageCreateInfo(format, usage, extent);
        imageInfo.pQueueFamilyIndices = &queueFamily;
        imageInfo.queueFamilyIndexCount = 1;
        imageInfo.imageType = type;
        imageInfo.initialLayout = layout;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VK_CHECK_RESULT(
            vmaCreateImage(
                m_GraphicsContext->GetAllocator(),
                &imageInfo,
                &allocInfo,
                &m_Image,
                &m_Allocation,
                nullptr
            )
        );

        const auto viewInfo = Initializers::ImageViewCreateInfo(
            format,
            m_Image,
            Converters::GetImageAspect(m_Aspect)
        );
        VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView));

        m_DescriptorInfo.imageLayout = layout;
        m_DescriptorInfo.imageView = m_ImageView;
    }
}