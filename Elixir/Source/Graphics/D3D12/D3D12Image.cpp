#include "epch.h"
#include "D3D12Image.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12Buffer.h>
#include <Graphics/D3D12/D3D12CommandBuffer.h>

namespace Elixir::D3D12
{
    namespace
    {
        using D3D12ImageTypes = std::tuple<
            D3D12BaseImage<Image>,
            D3D12BaseImage<DepthStencilImage>,
            D3D12BaseImage<Texture>,
            D3D12BaseImage<Texture2D>,
            D3D12BaseImage<Texture3D>
        >;

        template <size_t I = 0>
        ID3D12Resource* TryToGetD3D12ImageHandle(const Image* image)
        {
            if constexpr (I < std::tuple_size_v<D3D12ImageTypes>)
            {
                using ImageType = std::tuple_element_t<I, D3D12ImageTypes>;
                if (const auto img = dynamic_cast<const ImageType*>(image))
                    return img->GetD3D12Resource();
                return TryToGetD3D12ImageHandle<I + 1>(image);
            }
            else
            {
                EE_CORE_ASSERT(false, "Unsupported D3D12 image type!")
                return nullptr;
            }
        }

        D3D12_RESOURCE_DIMENSION GetResourceDimension(const EImageType type)
        {
            switch (type)
            {
                case EImageType::_1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                case EImageType::_2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                case EImageType::_3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                default: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            }
        }

        D3D12_SRV_DIMENSION GetSRVDimension(const Image* image)
        {
            switch (image->GetType())
            {
                case EImageType::_1D:
                    return image->GetArrayLayers() > 1
                        ? D3D12_SRV_DIMENSION_TEXTURE1DARRAY
                        : D3D12_SRV_DIMENSION_TEXTURE1D;
                case EImageType::_2D:
                    return image->GetArrayLayers() > 1
                        ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY
                        : D3D12_SRV_DIMENSION_TEXTURE2D;
                case EImageType::_3D:
                    return D3D12_SRV_DIMENSION_TEXTURE3D;
                default:
                    return D3D12_SRV_DIMENSION_TEXTURE2D;
            }
        }

        bool IsDepthStencilFormat(const EImageFormat format)
        {
            return format == EImageFormat::D16_UNORM ||
                format == EImageFormat::D24_UNORM_S8_UINT ||
                format == EImageFormat::D32_SFLOAT ||
                format == EImageFormat::D32_SFLOAT_S8_UINT ||
                format == EImageFormat::X8_D24_UNORM_PACK32;
        }
    }

    ID3D12Resource* TryToGetD3D12ImageResource(const Image* image)
    {
        return TryToGetD3D12ImageHandle(image);
    }

    template <typename Base>
    template <typename... Args>
    D3D12BaseImage<Base>::D3D12BaseImage(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        Args&&... args
    ) : Base(context, info, std::forward<Args>(args)...)
    {
        m_GraphicsContext = static_cast<const D3D12GraphicsContext*>(context);
    }

    template <typename Base>
    void D3D12BaseImage<Base>::Destroy()
    {
        m_Resource.Reset();
    }

    template <typename Base>
    void D3D12BaseImage<Base>::Resize(const Ref<CommandBuffer>&, const Extent3D extent)
    {
        if (extent.Width == 0 || extent.Height == 0)
            return;

        auto info = Base::GetCreateInfo();
        info.Width = extent.Width;
        info.Height = extent.Height;
        info.Depth = extent.Depth;
        info.InitialData = nullptr;

        Destroy();
        Base::m_Extent = extent;
        Base::RecalculateSize();
        CreateImage(info);
        CreateViews();
    }

    template <typename Base>
    void D3D12BaseImage<Base>::Transition(const CommandBuffer* cmd, const EImageLayout layout)
    {
        const auto newState = Converters::GetResourceState(layout);
        if (m_State == newState)
        {
            Base::m_Layout = layout;
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_Resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = m_State;
        barrier.Transition.StateAfter = newState;

        const auto d3dCmd = static_cast<const D3D12CommandBuffer*>(cmd);
        d3dCmd->GetD3D12CommandList()->ResourceBarrier(1, &barrier);

        m_State = newState;
        Base::m_Layout = layout;
    }

    template <typename Base>
    void D3D12BaseImage<Base>::Copy(
        const CommandBuffer* cmd,
        Image* dst,
        const Extent3D&,
        const Extent3D&
    )
    {
        const auto d3dCmd = static_cast<const D3D12CommandBuffer*>(cmd);
        d3dCmd->GetD3D12CommandList()->CopyResource(
            TryToGetD3D12ImageResource(dst),
            m_Resource.Get()
        );
    }

    template <typename Base>
    void D3D12BaseImage<Base>::CopyFrom(
        const CommandBuffer* cmd,
        const Buffer* src,
        std::span<SBufferImageCopy> regions
    )
    {
        const auto d3dCmd = static_cast<const D3D12CommandBuffer*>(cmd);

        SBufferImageCopy defaultRegion = SBufferImageCopy::Default(Base::m_Extent);
        if (regions.empty())
            regions = std::span<SBufferImageCopy>(&defaultRegion, 1);

        for (const auto& region : regions)
        {
            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
            dstLocation.pResource = m_Resource.Get();
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLocation.SubresourceIndex = region.ImageSubresource.MipLevel;

            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
            srcLocation.pResource = TryToGetD3D12Buffer(src);
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

            UINT rows = 0;
            UINT64 rowSize = 0;
            UINT64 totalSize = 0;
            const auto desc = m_Resource->GetDesc();
            m_GraphicsContext->GetDevice()->GetCopyableFootprints(
                &desc,
                region.ImageSubresource.MipLevel,
                1,
                region.BufferOffset,
                &srcLocation.PlacedFootprint,
                &rows,
                &rowSize,
                &totalSize
            );

            d3dCmd->GetD3D12CommandList()->CopyTextureRegion(
                &dstLocation,
                region.ImageOffset.X,
                region.ImageOffset.Y,
                region.ImageOffset.Z,
                &srcLocation,
                nullptr
            );
        }
    }

    template <typename Base>
    void D3D12BaseImage<Base>::CreateShaderResourceView(
        const D3D12_CPU_DESCRIPTOR_HANDLE destination
    ) const
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = Converters::GetFormat(Base::m_Format);
        desc.ViewDimension = GetSRVDimension(this);
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        switch (desc.ViewDimension)
        {
            case D3D12_SRV_DIMENSION_TEXTURE1D:
                desc.Texture1D.MipLevels = Base::m_MipLevels;
                break;
            case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
                desc.Texture1DArray.MipLevels = Base::m_MipLevels;
                desc.Texture1DArray.ArraySize = Base::m_ArrayLayers;
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2D:
                desc.Texture2D.MipLevels = Base::m_MipLevels;
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                desc.Texture2DArray.MipLevels = Base::m_MipLevels;
                desc.Texture2DArray.ArraySize = Base::m_ArrayLayers;
                break;
            case D3D12_SRV_DIMENSION_TEXTURE3D:
                desc.Texture3D.MipLevels = Base::m_MipLevels;
                break;
            default:
                break;
        }

        m_GraphicsContext->GetDevice()->CreateShaderResourceView(m_Resource.Get(), &desc, destination);
    }

    template <typename Base>
    void D3D12BaseImage<Base>::CreateImage(const SImageCreateInfo& info)
    {
        m_State = Converters::GetResourceState(info.InitialLayout);
        Base::m_Layout = info.InitialLayout;

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = GetResourceDimension(info.Type);
        desc.Alignment = 0;
        desc.Width = info.Width;
        desc.Height = info.Height;
        desc.DepthOrArraySize = info.Type == EImageType::_3D
            ? (UINT16)info.Depth
            : (UINT16)info.ArrayLayers;
        desc.MipLevels = (UINT16)info.MipLevels;
        desc.Format = Converters::GetFormat(info.Format);
        desc.SampleDesc = { 1, 0 };
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = Converters::GetResourceFlags(info.Usage);

        D3D12_CLEAR_VALUE clearValue = {};
        D3D12_CLEAR_VALUE* clear = nullptr;
        clearValue.Format = desc.Format;
        if (info.Usage & EImageUsage::DepthStencilAttachment)
        {
            clearValue.DepthStencil.Depth = 1.0f;
            clearValue.DepthStencil.Stencil = 0;
            clear = &clearValue;
        }
        else if (info.Usage & EImageUsage::ColorAttachment)
        {
            clearValue.Color[0] = 0.0f;
            clearValue.Color[1] = 0.0f;
            clearValue.Color[2] = 0.0f;
            clearValue.Color[3] = 1.0f;
            clear = &clearValue;
        }

        DX_CHECK_RESULT(
            m_GraphicsContext->GetDevice()->CreateCommittedResource(
                &heap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                m_State,
                clear,
                IID_PPV_ARGS(&m_Resource)
            )
        );

        if (info.InitialData)
            UploadInitialData(info);
    }

    template <typename Base>
    void D3D12BaseImage<Base>::CreateViews()
    {
        const auto format = Converters::GetFormat(Base::m_Format);

        if (Base::m_Usage & EImageUsage::ColorAttachment)
        {
            m_RTV = m_GraphicsContext->AllocateRTVDescriptor();
            D3D12_RENDER_TARGET_VIEW_DESC desc = {};
            desc.Format = format;
            desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            m_GraphicsContext->GetDevice()->CreateRenderTargetView(m_Resource.Get(), &desc, m_RTV.CPU);
        }

        if (Base::m_Usage & EImageUsage::DepthStencilAttachment)
        {
            m_DSV = m_GraphicsContext->AllocateDSVDescriptor();
            D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
            desc.Format = format;
            desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            desc.Flags = D3D12_DSV_FLAG_NONE;
            m_GraphicsContext->GetDevice()->CreateDepthStencilView(m_Resource.Get(), &desc, m_DSV.CPU);
        }
    }

    template <typename Base>
    void D3D12BaseImage<Base>::UploadInitialData(const SImageCreateInfo& info)
    {
        if (!info.InitialData || IsDepthStencilFormat(Base::m_Format))
            return;

        auto desc = m_Resource->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT rows = 0;
        UINT64 rowSize = 0;
        UINT64 totalSize = 0;
        m_GraphicsContext->GetDevice()->GetCopyableFootprints(
            &desc,
            0,
            1,
            0,
            &footprint,
            &rows,
            &rowSize,
            &totalSize
        );

        const auto upload = StagingBuffer::Create(m_GraphicsContext, totalSize);
        auto* mapped = static_cast<uint8_t*>(upload->Map());
        const auto* src = static_cast<const uint8_t*>(info.InitialData);
        const auto srcRowPitch = (UINT64)info.Width * Base::GetBytesPerPixel();

        for (UINT z = 0; z < std::max(1u, info.Depth); ++z)
        {
            for (UINT y = 0; y < rows; ++y)
            {
                Memory::Memcpy(
                    mapped + footprint.Offset + z * footprint.Footprint.RowPitch * rows + y * footprint.Footprint.RowPitch,
                    src + z * srcRowPitch * rows + y * srcRowPitch,
                    std::min(rowSize, srcRowPitch)
                );
            }
        }

        upload->Unmap();

        const auto cmd = m_GraphicsContext->GetUploadCommandBuffer();
        cmd->Begin();

        Transition(cmd.get(), EImageLayout::TransferDst);

        SBufferImageCopy region = SBufferImageCopy::Default(Base::m_Extent);
        CopyFrom(cmd.get(), upload.get(), std::span<SBufferImageCopy>(&region, 1));

        Transition(cmd.get(), info.InitialLayout);
        cmd->Flush();
    }

    D3D12Image::D3D12Image(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const void* data
    ) : D3D12Image(context, Image::CreateImageInfo(format, width, data)) {}

    D3D12Image::D3D12Image(const GraphicsContext* context, const SImageCreateInfo& info)
        : D3D12BaseImage(context, info)
    {
        CreateImage(info);
        CreateViews();
    }

    D3D12Image::~D3D12Image()
    {
        Destroy();
    }

    D3D12DepthStencilImage::D3D12DepthStencilImage(
        const GraphicsContext* context,
        const EDepthStencilImageFormat format,
        const uint32_t width,
        const uint32_t height
    ) : D3D12DepthStencilImage(context, DepthStencilImage::CreateImageInfo(format, width, height)) {}

    D3D12DepthStencilImage::D3D12DepthStencilImage(
        const GraphicsContext* context,
        const SImageCreateInfo& info
    ) : D3D12BaseImage(context, info)
    {
        CreateImage(info);
        CreateViews();
    }

    D3D12DepthStencilImage::~D3D12DepthStencilImage()
    {
        Destroy();
    }

    D3D12Texture::D3D12Texture(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const void* data,
        const std::string& path
    ) : D3D12Texture(context, Texture::CreateImageInfo(format, width, data, path), path) {}

    D3D12Texture::D3D12Texture(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : D3D12BaseImage(context, info, path)
    {
        CreateImage(info);
        CreateViews();
    }

    D3D12Texture::~D3D12Texture()
    {
        Destroy();
    }

    D3D12Texture2D::D3D12Texture2D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const void* data,
        const std::string& path
    ) : D3D12Texture2D(context, Texture2D::CreateImageInfo(format, width, height, data), path) {}

    D3D12Texture2D::D3D12Texture2D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : D3D12BaseImage(context, info, path)
    {
        CreateImage(info);
        CreateViews();
    }

    D3D12Texture2D::~D3D12Texture2D()
    {
        Destroy();
    }

    D3D12Texture3D::D3D12Texture3D(
        const GraphicsContext* context,
        const EImageFormat format,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        const void* data,
        const std::string& path
    ) : D3D12Texture3D(context, Texture3D::CreateImageInfo(format, width, height, depth, data), path) {}

    D3D12Texture3D::D3D12Texture3D(
        const GraphicsContext* context,
        const SImageCreateInfo& info,
        const std::string& path
    ) : D3D12BaseImage(context, info, path)
    {
        CreateImage(info);
        CreateViews();
    }

    D3D12Texture3D::~D3D12Texture3D()
    {
        Destroy();
    }
}

#endif
