#include "epch.h"
#include "Image.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Converters.h>
#include <Engine/Graphics/Utils.h>
#include <Graphics/Vulkan/VulkanImage.h>

namespace Elixir
{
    using namespace Elixir::Graphics;

    uint32_t CalculateBitsPerPixel(const Image* image)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto blockSize = Utils::GetFormatBlockSizeBits(image);
        const auto blockExtent = Utils::GetFormatBlockExtent(image);
        const uint32_t texels = blockExtent.x * blockExtent.y * blockExtent.z;

        return blockSize / texels;
    }

    size_t CalculateSize(const Image* image)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto extent = image->GetExtent();
        return extent.Width * extent.Height * extent.Depth * image->GetBitsPerPixel();
    }

    /* Image */

    void Image::Transition(const Ref<CommandBuffer>& cmd, const EImageLayout layout)
    {
        Transition(cmd.get(), layout);
    }

    void Image::Copy(const Ref<CommandBuffer>& cmd, const Ref<Image>& dst)
    {
        Copy(cmd.get(), dst);
    }

    void Image::Copy(
        const Ref<CommandBuffer>& cmd,
        const Ref<Image>& dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        Copy(cmd.get(), dst, srcExtent, dstExtent);
    }

    void Image::Copy(const CommandBuffer* cmd, const Ref<Image>& dst)
    {
        Copy(cmd, dst, GetExtent(), dst->GetExtent());
    }

    Ref<Image> Image::Create(
        const GraphicsContext* context,
        EImageFormat format,
        uint32_t width,
        void* data
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanImage>(context, format, width, data);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Image::Image(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        void* data
    ) : m_Format(format), m_Width(width), m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Type = EImageType::_1D;
        m_Layout = EImageLayout::Undefined;
        m_Usage = EImageUsage::Sampled | EImageUsage::TransferDst;
        m_Aspect = Utils::CalculateImageAspect(m_Usage, m_Format);
        m_BitsPerPixel = CalculateBitsPerPixel(this);
        m_Size = CalculateSize(this);
    }

    /* DepthStencilImage */

    Ref<DepthStencilImage> DepthStencilImage::Create(
        const GraphicsContext* context,
        EDepthStencilImageFormat format,
        uint32_t width,
        uint32_t height
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanDepthStencilImage>(
                    context,
                    format,
                    width,
                    height
                );
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    DepthStencilImage::DepthStencilImage(
        const GraphicsContext* context,
        const EDepthStencilImageFormat format,
        const uint32_t width,
        const uint32_t height
    ) : Image(context, Converters::GetImageFormat(format), width), m_Height(height)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Type = EImageType::_2D;
        m_Usage = EImageUsage::DepthStencilAttachment;
        m_Aspect = Utils::CalculateImageAspect(m_Usage, m_Format);
    }

    /* StorageImage */

    StorageImage::StorageImage(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height
    ) : Image(context, format, width), m_Height(height)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Type = EImageType::_2D;
        m_Layout = EImageLayout::General;
        m_Usage = EImageUsage::Storage |
            EImageUsage::TransferSrc |
            EImageUsage::TransferDst |
            EImageUsage::ColorAttachment;
        m_Aspect = Utils::CalculateImageAspect(m_Usage, m_Format);
    }
}