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

    namespace
    {
        using VulkanImageTypes = std::tuple<
            VulkanBaseImage<DepthStencilImage>,
            VulkanBaseImage<Image>
        >;

        template <size_t I = 0>
        VkImage TryToGetVulkanImageHandle(const Image* image)
        {
            if constexpr (I < std::tuple_size_v<VulkanImageTypes>)
            {
                using ImageType = std::tuple_element_t<I, VulkanImageTypes>;

                if (auto* img = dynamic_cast<const ImageType*>(image))
                {
                    return img->GetVulkanImage();
                }

                return TryToGetVulkanImageHandle<I + 1>(image);
            }

            EE_CORE_ASSERT(false, "Unsupported image type!")
            return VK_NULL_HANDLE;
        }
    }

    VkImage TryToGetVulkanImage(const Image* image)
    {
        if (!image) return VK_NULL_HANDLE;
        return TryToGetVulkanImageHandle(image);
    }

    /* VulkanBaseImage */

    template <typename Base>
    void VulkanBaseImage<Base>::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (!IsValid()) return;

        vkDestroyImageView(m_GraphicsContext->GetDevice(), m_ImageView, nullptr);
        vmaDestroyImage(m_GraphicsContext->GetAllocator(), m_Image, m_Allocation);
        m_Image = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;
        m_DescriptorInfo = {};
    }

    template <typename Base>
    void VulkanBaseImage<Base>::Transition(const CommandBuffer* cmd, const EImageLayout layout)
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
    void VulkanBaseImage<Base>::Copy(
        const CommandBuffer* cmd,
        const Image* dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Dst = TryToGetVulkanImage(dst);

        EE_CORE_ASSERT(vk_Dst != nullptr, "Invalid destination image!")
        EE_CORE_ASSERT(vk_Dst != VK_NULL_HANDLE, "Invalid destination image!")

        CommandUtils::CopyImageToImage(
            vk_Cmd->GetVulkanCommandBuffer(), m_Image, vk_Dst,
            Converters::GetExtent3D(srcExtent), Converters::GetExtent3D(dstExtent)
        );
    }

    template <typename Base>
    void VulkanBaseImage<Base>::CopyFrom(
        const CommandBuffer* cmd,
        const Buffer* src,
        const std::span<SBufferImageCopy> regions
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto vk_Cmd = static_cast<const VulkanCommandBuffer*>(cmd);
        const auto vk_Src = TryToGetVulkanBuffer(src);

        EE_CORE_ASSERT(vk_Src != VK_NULL_HANDLE, "Invalid source buffer!")

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
            vk_Cmd->GetVulkanCommandBuffer(), vk_Src, m_Image,
            Converters::GetImageLayout(this->GetLayout()), imageCopies.size(),
            imageCopies.data()
        );
    }

    template <typename Base>
    VulkanBaseImage<Base>::VulkanBaseImage(
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
    void VulkanBaseImage<Base>::CreateImage(const SImageCreateInfo& info)
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
    void VulkanBaseImage<Base>::InitImage(const SImageCreateInfo& info)
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
    void VulkanBaseImage<Base>::CreateImageView()
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
    void VulkanBaseImage<Base>::CreateDescriptorInfo()
    {
        EE_PROFILE_ZONE_SCOPED()
        m_DescriptorInfo = VkDescriptorImageInfo{};
        m_DescriptorInfo.imageLayout = Converters::GetImageLayout(this->GetLayout());
        m_DescriptorInfo.imageView = m_ImageView;
    }

    template <class Base>
    void VulkanBaseImage<Base>::UpdateSampler()
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
        : VulkanBaseImage(context, info)
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
        VulkanBaseImage::Destroy();
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
        : VulkanBaseImage(context, info)
    {
    }

    VulkanDepthStencilImage::~VulkanDepthStencilImage()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanBaseImage::Destroy();
    }

    template class VulkanBaseImage<Texture>;
    template class VulkanBaseImage<Texture2D>;
    template class VulkanBaseImage<Texture3D>;
}