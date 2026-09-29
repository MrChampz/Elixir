#include "epch.h"
#include "Image.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Utils.h>
#include <Graphics/Vulkan/VulkanImage.h>

#include <numeric>

namespace Elixir
{
    using namespace Elixir::Graphics;

    namespace
    {
        uint32_t CalculateBitsPerPixel(const Image* image)
        {
            const auto block = Utils::GetFormatBlockExtent(image);
            return Utils::GetFormatBlockSizeBits(image) / (block.x * block.y * block.z);
        }

        struct SNormalMapMipChain
        {
            std::vector<std::byte> Data;
            std::vector<SBufferImageCopy> Regions;
        };

        uint8_t EncodeNormalComponent(const float value)
        {
            return static_cast<uint8_t>(std::round(
                std::clamp(value * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f));
        }

        SNormalMapMipChain BuildNormalMapMipChain(const Image* image, const void* baseData)
        {
            EE_CORE_ASSERT(
                image->GetType() == EImageType::_2D,
                "Normal-map mip generation requires a 2D image."
            )
            EE_CORE_ASSERT(
                image->GetFormat() == EImageFormat::R8G8B8A8_UNORM,
                "Normal-map mip generation requires R8G8B8A8_UNORM data."
            )
            EE_CORE_ASSERT(
                image->GetArrayLayers() == 1,
                "Normal-map mip generation does not support array images."
            )

            SNormalMapMipChain result;
            uint32_t sourceWidth = image->GetWidth();
            uint32_t sourceHeight = image->GetExtent().Height;
            size_t sourceOffset = 0;

            for (uint32_t level = 0; level < image->GetMipLevels(); ++level)
            {
                const uint32_t destinationWidth = level == 0
                    ? sourceWidth
                    : std::max(1u, sourceWidth >> 1u);
                const uint32_t destinationHeight = level == 0
                    ? sourceHeight
                    : std::max(1u, sourceHeight >> 1u);
                const size_t destinationOffset = result.Data.size();
                const size_t destinationSize = size_t(destinationWidth) * destinationHeight * 4;

                result.Data.resize(destinationOffset + destinationSize);
                auto* destination = reinterpret_cast<uint8_t*>(
                    result.Data.data() + destinationOffset);

                if (level == 0)
                {
                    std::memcpy(destination, baseData, destinationSize);
                }
                else
                {
                    const auto* source = reinterpret_cast<const uint8_t*>(result.Data.data() + sourceOffset);

                    for (uint32_t y = 0; y < destinationHeight; ++y)
                    {
                        for (uint32_t x = 0; x < destinationWidth; ++x)
                        {
                            glm::vec3 averageNormal(0.0f);
                            float averageAlpha = 0.0f;

                            for (uint32_t offsetY = 0; offsetY < 2; ++offsetY)
                            {
                                for (uint32_t offsetX = 0; offsetX < 2; ++offsetX)
                                {
                                    const uint32_t sampleX = std::min(x * 2 + offsetX, sourceWidth - 1);
                                    const uint32_t sampleY = std::min(y * 2 + offsetY, sourceHeight - 1);
                                    const auto* sample = source + (sampleY * sourceWidth + sampleX) * 4;
                                    averageNormal += glm::vec3(sample[0], sample[1], sample[2]) / 255.0f * 2.0f - 1.0f;
                                    averageAlpha += static_cast<float>(sample[3]);
                                }
                            }

                            const float normalLengthSquared = glm::dot(averageNormal, averageNormal);
                            const glm::vec3 normal = normalLengthSquared > 0.0f
                                ? averageNormal * glm::inversesqrt(normalLengthSquared)
                                : glm::vec3(0.0f, 0.0f, 1.0f);
                            auto* pixel = destination + (y * destinationWidth + x) * 4;
                            pixel[0] = EncodeNormalComponent(normal.x);
                            pixel[1] = EncodeNormalComponent(normal.y);
                            pixel[2] = EncodeNormalComponent(normal.z);
                            pixel[3] = static_cast<uint8_t>(std::round(averageAlpha * 0.25f));
                        }
                    }
                }

                SBufferImageCopy region = {};
                region.BufferOffset = destinationOffset;
                region.ImageSubresource.AspectMask = EImageAspect::Color;
                region.ImageSubresource.MipLevel = level;
                region.ImageExtent = { destinationWidth, destinationHeight, 1 };
                result.Regions.push_back(region);

                sourceOffset = destinationOffset;
                sourceWidth = destinationWidth;
                sourceHeight = destinationHeight;
            }

            return result;
        }

    }

    void Image::Transition(const Ref<CommandBuffer>& cmd, const EImageLayout layout)
    {
        Transition(cmd.get(), layout);
    }

    void Image::Copy(const Ref<CommandBuffer>& cmd, const Ref<Image>& dst)
    {
        Copy(cmd.get(), dst.get());
    }

    void Image::Copy(const Ref<CommandBuffer>& cmd, Image* dst)
    {
        Copy(cmd.get(), dst);
    }

    void Image::Copy(const CommandBuffer* cmd, const Ref<Image>& dst)
    {
        Copy(cmd, dst.get());
    }

    void Image::Copy(const CommandBuffer* cmd, Image* dst)
    {
        Copy(cmd, dst, GetExtent(), dst->GetExtent());
    }

    void Image::Copy(
        const Ref<CommandBuffer>& cmd,
        const Ref<Image>& dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        Copy(cmd.get(), dst.get(), srcExtent, dstExtent);
    }

    void Image::Copy(
        const Ref<CommandBuffer>& cmd,
        Image* dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        Copy(cmd.get(), dst, srcExtent, dstExtent);
    }

    void Image::Copy(
        const CommandBuffer* cmd,
        const Ref<Image>& dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        Copy(cmd, dst.get(), srcExtent, dstExtent);
    }

    void Image::CopyFrom(
        const Ref<CommandBuffer>& cmd,
        const Ref<Buffer>& src,
        const std::span<SBufferImageCopy> regions
    )
    {
        CopyFrom(cmd.get(), src.get(), regions);
    }

    void Image::CopyFrom(
        const Ref<CommandBuffer>& cmd,
        const Buffer* src,
        const std::span<SBufferImageCopy> regions
    )
    {
        CopyFrom(cmd.get(), src, regions);
    }

    void Image::CopyFrom(
        const CommandBuffer* cmd,
        const Ref<Buffer>& src,
        const std::span<SBufferImageCopy> regions
    )
    {
        CopyFrom(cmd, src.get(), regions);
    }

    void Image::Copy(
        const CommandBuffer* cmd,
        Image* dst,
        const Extent3D& srcExtent,
        const Extent3D& dstExtent
    )
    {
        CopyMip(cmd, dst, srcExtent, dstExtent, 0);
    }

    Extent3D Image::GetMipExtent(const uint32_t level) const
    {
        EE_CORE_ASSERT(level < m_MipLevels, "Image mip level is outside the allocated chain.")
        if (level >= m_MipLevels)
            return { 0, 0, 0 };

        return {
            std::max(1u, m_Extent.Width >> level),
            std::max(1u, m_Extent.Height >> level),
            std::max(1u, m_Extent.Depth >> level)
        };
    }

    size_t Image::GetMipSize(const uint32_t level) const
    {
        const auto extent = GetMipExtent(level);
        const auto block = Utils::GetFormatBlockExtent(this);
        return ((size_t(extent.Width) + block.x - 1) / block.x) *
            ((size_t(extent.Height) + block.y - 1) / block.y) *
            ((size_t(extent.Depth) + block.z - 1) / block.z) *
            (Utils::GetFormatBlockSizeBits(this) / CHAR_BIT);
    }

    SImageCreateInfo Image::GetCreateInfo() const
    {
        return {
            .Width = m_Extent.Width,
            .Height = m_Extent.Height,
            .Depth = m_Extent.Depth,
            .Type = m_Type,
            .Format = m_Format,
            .MipmapMode = EImageMipmapMode::LeaveExistingMips,
            .MipLevels = m_MipLevels,
            .ArrayLayers = m_ArrayLayers,
            .Usage = m_Usage,
            .InitialLayout = m_Layout,
            .AllocationInfo = m_AllocationInfo,
        };
    }

    uint32_t Image::GetFullMipLevelCount(const Extent3D& extent)
    {
        uint32_t largestDimension = std::max({ extent.Width, extent.Height, extent.Depth });
        uint32_t levelCount = 1;
        while (largestDimension > 1)
        {
            largestDimension >>= 1;
            ++levelCount;
        }
        return levelCount;
    }

    uint32_t Image::GetMipLevelCount(const SImageCreateInfo& info)
    {
        EE_CORE_ASSERT(
            info.Width > 0 && info.Height > 0 && info.Depth > 0,
            "Image dimensions must be greater than zero."
        )
        if (!info.Width || !info.Height || !info.Depth)
            return 0;

        const auto count = GetFullMipLevelCount({ info.Width, info.Height, info.Depth });
        switch (info.MipmapMode)
        {
            case EImageMipmapMode::NoMipmaps:
                return 1;
            case EImageMipmapMode::SimpleAverage:
            case EImageMipmapMode::NormalMap:
                return count;
            case EImageMipmapMode::LeaveExistingMips:
                EE_CORE_ASSERT(
                    info.MipLevels > 0 && info.MipLevels <= count,
                    "The supplied mip count does not fit the image extent."
                )
                if (info.MipLevels == 0 || info.MipLevels > count)
                    return 0;
                return info.MipLevels;
        }

        EE_CORE_ERROR("Unknown image mipmap mode.")
        return 0;
    }

    Ref<Image> Image::Create(const GraphicsContext* context, SImageCreateInfo info)
    {
        info.MipLevels = GetMipLevelCount(info);
        if (info.MipLevels == 0)
            return nullptr;

        if (info.Format == EImageFormat::Undefined || !info.ArrayLayers)
        {
            EE_CORE_ERROR("An image requires a format and at least one array layer.")
            return nullptr;
        }

        if (info.InitialLayout == EImageLayout::PreInitialized)
        {
            EE_CORE_ERROR("PreInitialized is not a final layout for an image created by upload.")
            return nullptr;
        }

        if ((info.Type == EImageType::_1D && (info.Height != 1 || info.Depth != 1)) ||
            (info.Type == EImageType::_2D && info.Depth != 1) ||
            (info.Type == EImageType::_3D && info.ArrayLayers != 1))
        {
            EE_CORE_ERROR("Image dimensions and layers do not match its type.")
            return nullptr;
        }

        const bool generatesMips = info.MipmapMode == EImageMipmapMode::SimpleAverage ||
            info.MipmapMode == EImageMipmapMode::NormalMap;
        if (generatesMips && !info.InitialData)
        {
            EE_CORE_ERROR("Mip generation requires base-level pixels.")
            return nullptr;
        }

        if (info.InitialData && !info.InitialMipData.empty())
        {
            EE_CORE_ERROR("Use either InitialData or InitialMipData, not both.")
            return nullptr;
        }

        if (info.MipmapMode == EImageMipmapMode::LeaveExistingMips &&
            info.MipLevels > 1 && info.InitialData)
        {
            EE_CORE_ERROR("Use InitialMipData to supply an existing mip chain.")
            return nullptr;
        }

        if (info.MipmapMode == EImageMipmapMode::NormalMap &&
           (info.Type != EImageType::_2D || info.ArrayLayers != 1 ||
            info.Format != EImageFormat::R8G8B8A8_UNORM))
        {
            EE_CORE_ERROR("Normal-map mips require a single R8G8B8A8_UNORM 2D image.")
            return nullptr;
        }

        if (info.InitialData || !info.InitialMipData.empty())
        {
            info.Usage |= EImageUsage::TransferDst;
            if (info.InitialLayout == EImageLayout::Undefined)
                info.InitialLayout = EImageLayout::General;
        }

        if (info.MipmapMode == EImageMipmapMode::SimpleAverage && info.MipLevels > 1)
            info.Usage |= EImageUsage::TransferSrc;

        if (!context)
        {
            EE_CORE_ERROR("Image creation requires a graphics context.")
            return nullptr;
        }

        Ref<Image> image;
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                image = CreateRef<Vulkan::VulkanImage>(context, info);
                break;
            default:
                EE_CORE_ERROR("Unknown graphics API.")
                return nullptr;
        }

        image->Initialize(info);

        return image->IsValid() ? image : nullptr;
    }

    Ref<Image> Image::Create(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const void* data
    )
    {
        return Create(context, CreateImageInfo(format, width, data));
    }

    SImageCreateInfo Image::CreateImageInfo(
        const EImageFormat format,
        const uint32_t width,
        const void* data
    )
    {
        return {
            .Width = width,
            .Type = EImageType::_1D,
            .Format = format,
            .Usage = EImageUsage::Sampled,
            .InitialData = data,
        };
    }

    Image::Image(const GraphicsContext* context, const SImageCreateInfo& info)
        : m_Type(info.Type),
          m_Layout(EImageLayout::Undefined),
          m_Format(info.Format),
          m_Usage(info.Usage),
          m_Aspect(Utils::CalculateImageAspect(info.Usage, info.Format)),
          m_Extent(info.Width, info.Height, info.Depth),
          m_MipLevels(info.MipLevels),
          m_ArrayLayers(info.ArrayLayers),
          m_AllocationInfo(info.AllocationInfo),
          m_GraphicsContext(context)
    {
        RecalculateSize();
#ifdef EE_DEBUG
        m_DebugName = "Image[" + m_UUID.ToString() + "]";
#endif
    }

    void Image::Initialize(const SImageCreateInfo& info)
    {
        SNormalMapMipChain upload;
        const void* pixels = info.InitialData;
        size_t uploadSize = GetSize();

        if (info.MipmapMode == EImageMipmapMode::NormalMap)
        {
            upload = BuildNormalMapMipChain(this, pixels);
            pixels = upload.Data.data();
            uploadSize = upload.Data.size();
        }
        else if (!info.InitialMipData.empty())
        {
            const auto count = static_cast<size_t>(m_MipLevels) * m_ArrayLayers;
            if (info.InitialMipData.size() != count)
            {
                EE_CORE_ERROR("InitialMipData must contain every mip level and array layer.")
                return;
            }

            std::vector supplied(count, false);
            const size_t alignment = std::lcm(
                size_t{4},
                size_t(Utils::GetFormatBlockSizeBits(this) / CHAR_BIT)
            );

            for (const auto& mip : info.InitialMipData)
            {
                if (!mip.Data || mip.MipLevel >= m_MipLevels || mip.ArrayLayer >= m_ArrayLayers ||
                    mip.Size != GetMipSize(mip.MipLevel))
                {
                    EE_CORE_ERROR("Invalid initial mip subresource.")
                    return;
                }

                const size_t index = static_cast<size_t>(mip.ArrayLayer) * m_MipLevels + mip.MipLevel;
                if (supplied[index])
                {
                    EE_CORE_ERROR("Duplicate initial mip subresource.")
                    return;
                }

                supplied[index] = true;

                const size_t offset = (upload.Data.size() + alignment - 1) / alignment * alignment;
                upload.Data.resize(offset + mip.Size);
                std::memcpy(upload.Data.data() + offset, mip.Data, mip.Size);

                SBufferImageCopy region{};
                region.BufferOffset = offset;
                region.ImageSubresource = { m_Aspect, mip.MipLevel, mip.ArrayLayer, 1 };
                region.ImageExtent = GetMipExtent(mip.MipLevel);

                upload.Regions.push_back(region);
            }

            pixels = upload.Data.data();
            uploadSize = upload.Data.size();
        }
        else if (pixels)
        {
            SBufferImageCopy region{};
            region.ImageSubresource = { m_Aspect, 0, 0, m_ArrayLayers };
            region.ImageExtent = m_Extent;
            upload.Regions.push_back(region);
        }

        if (pixels && (m_Aspect & EImageAspect::Depth) && (m_Aspect & EImageAspect::Stencil))
        {
            EE_CORE_ERROR("Combined depth/stencil uploads require separate aspect regions.")
            return;
        }

        CreateResource(info);

        if (!pixels && info.InitialLayout == EImageLayout::Undefined)
            return;

        Ref<StagingBuffer> staging;
        if (pixels)
            staging = StagingBuffer::Create(m_GraphicsContext, uploadSize, pixels);

        const auto cmd = m_GraphicsContext->GetUploadCommandBuffer();
        cmd->Begin();

        if (staging)
        {
            Transition(cmd, EImageLayout::TransferDst);
            CopyFrom(cmd, staging, upload.Regions);
        }

        if (info.MipmapMode == EImageMipmapMode::SimpleAverage && m_MipLevels > 1)
            GenerateMipmaps(cmd.get(), info.InitialLayout);
        else
            Transition(cmd, info.InitialLayout);

        cmd->Flush();
    }

    void Image::Resize(const Extent3D& extent)
    {
        if (m_GraphicsContext->IsRenderThread())
        {
            m_GraphicsContext->WaitDeviceIdle();
            ResizeOnRenderThread(m_GraphicsContext->GetUploadCommandBuffer(), extent);
            return;
        }

        {
            std::scoped_lock lock(m_ResizeMutex);
            m_PendingResize = extent;
            if (m_ResizeQueued)
                return;

            m_ResizeQueued = true;
        }

        const auto enqueued = m_GraphicsContext->EnqueueRenderTask(
            [self = shared_from_this()]()
            {
                self->ApplyPendingResize();
            }
        );

        if (!enqueued)
        {
            std::scoped_lock lock(m_ResizeMutex);
            m_ResizeQueued = false;
            EE_CORE_ERROR("Could not queue image resize because the graphics context is unavailable.")
        }
    }

    bool Image::ResizeAndWait(const Extent3D& extent)
    {
        return m_GraphicsContext->RunRenderTaskAndWait(
            [self = shared_from_this(), extent]()
            {
                self->m_GraphicsContext->WaitDeviceIdle();
                self->ResizeOnRenderThread(
                    self->m_GraphicsContext->GetUploadCommandBuffer(),
                    extent
                );
            }
        );
    }

    void Image::RecalculateSize()
    {
        m_BitsPerPixel = CalculateBitsPerPixel(this);
        m_Size = GetMipSize(0) * m_ArrayLayers;
    }

    void Image::ApplyPendingResize()
    {
        while (true)
        {
            std::optional<Extent3D> extent;
            {
                std::scoped_lock lock(m_ResizeMutex);
                extent.swap(m_PendingResize);
                if (!extent)
                {
                    m_ResizeQueued = false;
                    return;
                }
            }

            m_GraphicsContext->WaitDeviceIdle();
            ResizeOnRenderThread(m_GraphicsContext->GetUploadCommandBuffer(), *extent);
        }
    }

    void Image::ResizeOnRenderThread(const Ref<CommandBuffer>& cmd, const Extent3D& extent)
    {
        if (!extent.Width || !extent.Height || !extent.Depth)
        {
            EE_CORE_ERROR("Image dimensions must be greater than zero.")
            return;
        }

        if (extent.Width == m_Extent.Width && extent.Height == m_Extent.Height &&
            extent.Depth == m_Extent.Depth)
            return;

        if ((m_Type == EImageType::_1D && (extent.Height != 1 || extent.Depth != 1)) ||
            (m_Type == EImageType::_2D && extent.Depth != 1))
        {
            EE_CORE_ERROR("Resize extent does not match the image type.")
            return;
        }

        if (!(m_Usage & EImageUsage::TransferSrc) || !(m_Usage & EImageUsage::TransferDst))
        {
            EE_CORE_ERROR("Preserving image contents requires both transfer usages.")
            return;
        }

        auto info = GetCreateInfo();
        info.InitialLayout = EImageLayout::TransferDst;

        const auto staging = Create(m_GraphicsContext, info);
        if (!staging)
            return;

        const auto originalLayout = m_Layout;
        const bool preserve = originalLayout != EImageLayout::Undefined;

        if (preserve)
        {
            cmd->Begin();
            Transition(cmd, EImageLayout::TransferSrc);

            for (uint32_t level = 0; level < m_MipLevels; ++level)
                CopyMip(
                    cmd.get(),
                    staging.get(),
                    GetMipExtent(level),
                    GetMipExtent(level),
                    level
                );

            staging->Transition(cmd, EImageLayout::TransferSrc);
            cmd->Flush();
        }

        info.Width = extent.Width;
        info.Height = extent.Height;
        info.Depth = extent.Depth;
        info.MipLevels = std::min(m_MipLevels, GetFullMipLevelCount(extent));
        info.MipLevels = GetMipLevelCount(info);
        info.InitialLayout = EImageLayout::Undefined;

        Destroy();
        m_Extent = extent;
        m_MipLevels = info.MipLevels;
        m_Layout = EImageLayout::Undefined;
        RecalculateSize();
        CreateResource(info);

        if (preserve)
        {
            cmd->Begin();
            Transition(cmd, EImageLayout::TransferDst);

            for (uint32_t level = 0; level < m_MipLevels; ++level)
                staging->CopyMip(
                    cmd.get(),
                    this,
                    staging->GetMipExtent(level),
                    GetMipExtent(level),
                    level
                );

            Transition(cmd, originalLayout);
            cmd->Flush();
        }
    }
}
