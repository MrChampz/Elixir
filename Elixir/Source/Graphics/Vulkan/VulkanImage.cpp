#include "epch.h"
#include "VulkanImage.h"

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Converters.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
#include <Graphics/Vulkan/VulkanSampler.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include <Graphics/Vulkan/Converters.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    using namespace Elixir::Graphics::Converters;
    using namespace Elixir::Graphics::Utils;
    using namespace Elixir::Vulkan;

    /* VulkanImageBase */

    template <typename Base>
    void VulkanImageBase<Base>::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_Destroyed) return;

        vkDestroyImageView(m_GraphicsContext->GetDevice(), m_ImageView, nullptr);
        vmaDestroyImage(m_GraphicsContext->GetAllocator(), m_Image, m_Allocation);
        m_Destroyed = true;
    }

    template <typename Base>
    void VulkanImageBase<Base>::Transition(const CommandBuffer* cmd, const EImageLayout layout)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);

        CommandUtils::TransitionImage(
            vk_Cmd->GetVulkanCommandBuffer(), m_Image,
            Converters::GetImageLayout(this->GetLayout()),
            Converters::GetImageLayout(layout),
            Converters::GetImageAspect(this->GetAspect())
        );

        this->m_Layout = layout;
    }

    template <typename Base>
    void VulkanImageBase<Base>::Copy(
        const CommandBuffer* cmd,
        const Image* dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Dst = GetVulkanImageHandler(dst);

        EE_CORE_ASSERT(vk_Dst != nullptr, "Invalid destination image!")
        EE_CORE_ASSERT(vk_Dst != VK_NULL_HANDLE, "Invalid destination image!")

        CommandUtils::CopyImageToImage(
            vk_Cmd->GetVulkanCommandBuffer(), m_Image, vk_Dst,
            Converters::GetExtent3D(srcExtent), Converters::GetExtent3D(dstExtent)
        );
    }

    template <typename Base>
    void VulkanImageBase<Base>::CopyFrom(
        const CommandBuffer* cmd,
        const Buffer* src,
        const std::span<SBufferImageCopy> regions
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Src = dynamic_cast<const VulkanBaseBuffer*>(src);

        EE_CORE_ASSERT(vk_Src != nullptr, "Invalid source buffer!")
        EE_CORE_ASSERT(vk_Src->GetVulkanBuffer() != VK_NULL_HANDLE, "Invalid source buffer!")

        std::span copyRegions = regions;

        if (copyRegions.empty())
        {
            SBufferImageCopy defaultRegion[] = {
                SBufferImageCopy::Default(this->GetExtent())
            };
            copyRegions = std::span(defaultRegion);
        }

        std::vector<VkBufferImageCopy> imageCopies(copyRegions.size());

        std::ranges::transform(
            copyRegions, imageCopies.begin(), Converters::GetBufferImageCopy
        );

        vkCmdCopyBufferToImage(
            vk_Cmd->GetVulkanCommandBuffer(), vk_Src->GetVulkanBuffer(), m_Image,
            Converters::GetImageLayout(this->GetLayout()), imageCopies.size(),
            imageCopies.data()
        );
    }

    template <typename Base>
    VulkanImageBase<Base>::VulkanImageBase(
        const GraphicsContext* context,
        const SImageCreateInfo& info
    ) : Base(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()

        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);

        CreateImage(info);
        InitImage(info);
        CreateImageView();
        CreateDescriptorInfo();
    }

    template <typename Base>
    void VulkanImageBase<Base>::CreateImage(const SImageCreateInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        const uint32_t queueFamily = m_GraphicsContext->GetGraphicsQueueFamily();

        const auto imageInfo = Initializers::ImageCreateInfo(info, queueFamily);
        const auto allocInfo = Initializers::AllocationCreateInfo(info.AllocationInfo);

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
    }

    template <typename Base>
    void VulkanImageBase<Base>::InitImage(const SImageCreateInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        auto& cmd = m_GraphicsContext->GetCommandBuffer();   // TODO: Should have a TRANSFER only command buffer
        auto vk_Cmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

        cmd->Begin();

        if (info.InitialData)
        {
            this->Transition(cmd.get(), EImageLayout::TransferDst);

            const auto stagingBuffer = StagingBuffer::Create(
                m_GraphicsContext,
                this->GetSize(),
                info.InitialData
            );

            SBufferImageCopy copyRegion = {};
            copyRegion.ImageSubresource.AspectMask = this->GetAspect();
            copyRegion.ImageSubresource.LayerCount = this->GetArrayLayers();
            copyRegion.ImageExtent = this->GetExtent();

            SBufferImageCopy regions[] = { copyRegion };
            this->CopyFrom(cmd, stagingBuffer, regions);

            this->Transition(cmd.get(), info.InitialLayout);
            cmd->Flush();

            stagingBuffer->Destroy();
        }
        else
        {
            this->Transition(cmd.get(), info.InitialLayout);
            cmd->Flush();
        }
    }

    template <typename Base>
    void VulkanImageBase<Base>::CreateImageView()
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto viewInfo = Initializers::ImageViewCreateInfo(this);
        VK_CHECK_RESULT(
            vkCreateImageView(
                m_GraphicsContext->GetDevice(),
                &viewInfo,
                nullptr,
                &m_ImageView
            )
        );
    }

    template <typename Base>
    void VulkanImageBase<Base>::CreateDescriptorInfo()
    {
        EE_PROFILE_ZONE_SCOPED()
        m_DescriptorInfo = VkDescriptorImageInfo{};
        m_DescriptorInfo.imageLayout = Converters::GetImageLayout(this->GetLayout());
        m_DescriptorInfo.imageView = m_ImageView;
    }

    template <class Base>
    void VulkanImageBase<Base>::UpdateSampler()
    {
        auto vk_Sampler = std::static_pointer_cast<VulkanSampler>(this->GetSampler());
        m_DescriptorInfo.sampler = vk_Sampler->GetVulkanSampler();
    }

    /* VulkanImage */

    VulkanImage::VulkanImage(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        void* data
    ) : VulkanImage(context, CreateImageInfo(format, width, data)) {}

    VulkanImage::VulkanImage(const GraphicsContext* context, const SImageCreateInfo& info)
        : VulkanImageBase(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()

        // TODO: Set a default sampler
        // auto sampler = SamplerBuilder()
        //     .SetMagFilter(ESamplerFilter::Nearest)
        //     .SetMinFilter(ESamplerFilter::Nearest)
        //     .Build();

        // SetSampler(sampler);
    }

    VulkanImage::~VulkanImage()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanImageBase::Destroy();
    }

    /* VulkanDepthStencilImage */

    VulkanDepthStencilImage::VulkanDepthStencilImage(
        const GraphicsContext* context,
        const EDepthStencilImageFormat format,
        const uint32_t width,
        const uint32_t height
    ) : VulkanDepthStencilImage(context, CreateImageInfo(format, width, height)) {}

    VulkanDepthStencilImage::VulkanDepthStencilImage(
        const GraphicsContext* context,
        const SImageCreateInfo& info
    )
        : VulkanImageBase(context, info)
    {
    }

    VulkanDepthStencilImage::~VulkanDepthStencilImage()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanImageBase::Destroy();
    }

    template class VulkanImageBase<Texture>;
    template class VulkanImageBase<Texture2D>;
    template class VulkanImageBase<Texture3D>;
}