#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/GraphicsContext.h>

namespace Elixir
{
    class Image;

    namespace Vulkan
    {
        template <typename>
        class VulkanImageBase;
    }

    enum class EImageLayout
    {
        Undefined,
		General,
		ColorAttachment,
		DepthAttachment,
		StencilAttachment,
		DepthStencilAttachment,
		DepthReadOnly,
		StencilReadOnly,
		DepthStencilReadOnly,
		ShaderReadOnly,
		TransferSrc,
		TransferDst,
		PreInitialized,
		PresentSrc
    };

    enum class EImageUsage
	{
		TransferSrc				= 0x00000001,
		TransferDst				= 0x00000002,
		Sampled					= 0x00000004,
		Storage					= 0x00000008,
		ColorAttachment			= 0x00000010,
		DepthStencilAttachment	= 0x00000020,
		TransientAttachment		= 0x00000040,
		InputAttachment			= 0x00000080
	};

    GENERATE_ENUM_CLASS_OPERATORS(EImageUsage)

    enum class EImageAspect
	{
		Color		= 0x00000001,
		Depth		= 0x00000002,
		Stencil		= 0x00000004,
		Metadata	= 0x00000008,
		Plane0		= 0x00000010,
		Plane1		= 0x00000020,
		Plane2		= 0x00000040,
		None		= 0,
	};

	GENERATE_ENUM_CLASS_OPERATORS(EImageAspect)

	enum class EImageType
	{
		_1D, _2D, _3D
	};

	enum class EImageFormat
	{
	    R16_SFLOAT,
	    R8G8B8_UNORM,
        R8G8B8_SNORM,
        R8G8B8_USCALED,
        R8G8B8_SSCALED,
        R8G8B8_UINT,
        R8G8B8_SINT,
        R8G8B8_SRGB,
	    R16G16B16_UNORM,
        R16G16B16_SNORM,
        R16G16B16_USCALED,
        R16G16B16_SSCALED,
        R16G16B16_UINT,
        R16G16B16_SINT,
	    R16G16B16_SFLOAT,
	    R32G32B32_UINT,
        R32G32B32_SINT,
        R32G32B32_SFLOAT,
	    R64G64B64_UINT,
        R64G64B64_SINT,
        R64G64B64_SFLOAT,
	    R8G8B8A8_UNORM,
        R8G8B8A8_SNORM,
        R8G8B8A8_USCALED,
        R8G8B8A8_SSCALED,
        R8G8B8A8_UINT,
        R8G8B8A8_SINT,
        R8G8B8A8_SRGB,
	    R16G16B16A16_UNORM,
        R16G16B16A16_SNORM,
        R16G16B16A16_USCALED,
        R16G16B16A16_SSCALED,
        R16G16B16A16_UINT,
        R16G16B16A16_SINT,
        R16G16B16A16_SFLOAT,
	    R32G32B32A32_UINT,
        R32G32B32A32_SINT,
        R32G32B32A32_SFLOAT,
	    R64G64B64A64_UINT,
        R64G64B64A64_SINT,
        R64G64B64A64_SFLOAT,
	    BC1_RGB_UNORM_BLOCK,
        BC1_RGB_SRGB_BLOCK,
	    BC1_RGBA_UNORM_BLOCK,
        BC1_RGBA_SRGB_BLOCK,
	    BC2_UNORM_BLOCK,
        BC2_SRGB_BLOCK,
	    BC3_UNORM_BLOCK,
	    BC3_SRGB_BLOCK,
	    BC4_UNORM_BLOCK,
        BC4_SNORM_BLOCK,
	    BC5_UNORM_BLOCK,
        BC5_SNORM_BLOCK,
	    BC6H_UFLOAT_BLOCK,
        BC6H_SFLOAT_BLOCK,
	    BC7_UNORM_BLOCK,
        BC7_SRGB_BLOCK,
		D16_UNORM = 500,
		D24_UNORM_S8_UINT = 501,
		D32_SFLOAT = 502,
		D32_SFLOAT_S8_UINT = 503,
		X8_D24_UNORM_PACK32 = 504,
		Undefined
	};

	enum class EDepthStencilImageFormat
	{
		D16_UNORM = 500,
		D24_UNORM_S8_UINT = 501,
		D32_SFLOAT = 502,
		D32_SFLOAT_S8_UINT = 503,
		X8_D24_UNORM_PACK32 = 504
	};

    struct SImageLayeredSubresource
    {
        EImageAspect AspectMask;
        uint32_t MipLevel = 0;
        uint32_t BaseArrayLayer = 0;
        uint32_t LayerCount = 1;

        static SImageLayeredSubresource Default()
        {
            return {
                .AspectMask = EImageAspect::Color,
            };
        }
    };

    struct SBufferImageCopy
    {
        uint64_t BufferOffset = 0;
        uint32_t BufferRowLength = 0;
        uint32_t BufferImageHeight = 0;
        SImageLayeredSubresource ImageSubresource;
        Offset3D ImageOffset;
        Extent3D ImageExtent;

        static SBufferImageCopy Default(const Extent3D& extent = Extent3D())
        {
            return {
                .ImageSubresource = SImageLayeredSubresource::Default(),
                .ImageExtent = extent,
            };
        }
    };

    struct SImageCreateInfo
    {
        void* InitialData = nullptr;
        uint32_t Width = 0;
        uint32_t Height = 1;
        uint32_t Depth = 1;
        EImageType Type;
        EImageFormat Format;
        uint32_t MipLevels = 1;
        uint32_t ArrayLayers = 1;
        EImageUsage Usage;
        EImageLayout InitialLayout = EImageLayout::Undefined;
        SAllocationInfo AllocationInfo;
    };

    class ELIXIR_API Image
    {
        friend class TextureLoader;

        template <typename>
        friend class Vulkan::VulkanImageBase;
    public:
        virtual ~Image() = default;

        virtual void Destroy() = 0;

        void Transition(const Ref<CommandBuffer>& cmd, EImageLayout layout);
        virtual void Transition(const CommandBuffer* cmd, EImageLayout layout) = 0;

        void Copy(const Ref<CommandBuffer>& cmd, const Ref<Image>& dst);
        void Copy(const Ref<CommandBuffer>& cmd, const Image* dst);
        void Copy(const CommandBuffer* cmd, const Ref<Image>& dst);
        void Copy(const CommandBuffer* cmd, const Image* dst);

        void Copy(
            const Ref<CommandBuffer>& cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );
        void Copy(
            const Ref<CommandBuffer>& cmd,
            const Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );
        void Copy(
            const CommandBuffer* cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );
        virtual void Copy(
            const CommandBuffer* cmd,
            const Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        ) = 0;

        void CopyFrom(
            const Ref<CommandBuffer>& cmd,
            const Ref<Buffer>& src,
            std::span<SBufferImageCopy> regions = {}
        );
        void CopyFrom(
            const Ref<CommandBuffer>& cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        );
        void CopyFrom(
            const CommandBuffer* cmd,
            const Ref<Buffer>& src,
            std::span<SBufferImageCopy> regions = {}
        );
        virtual void CopyFrom(
            const CommandBuffer* cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        ) = 0;

        [[nodiscard]] virtual bool IsValid() const = 0;

        [[nodiscard]] const UUID& GetUUID() const { return m_UUID; }

        [[nodiscard]] EImageType GetType() const { return m_Type; }
        [[nodiscard]] EImageLayout GetLayout() const { return m_Layout; }
        [[nodiscard]] EImageFormat GetFormat() const { return m_Format; }
        [[nodiscard]] EImageUsage GetUsage() const { return m_Usage; }
        [[nodiscard]] EImageAspect GetAspect() const { return m_Aspect; }

        [[nodiscard]] Extent3D GetExtent() const { return m_Extent; }
        [[nodiscard]] uint32_t GetWidth() const { return m_Extent.Width; }

        [[nodiscard]] uint32_t GetMipLevels() const { return m_MipLevels; }
        [[nodiscard]] uint32_t GetArrayLayers() const { return m_ArrayLayers; }

        [[nodiscard]] uint32_t GetBitsPerPixel() const { return m_BitsPerPixel; }
        [[nodiscard]] size_t GetSize() const { return m_Size; }

        [[nodiscard]] bool IsHDR() const { return m_HDR; }

        [[nodiscard]] const Ref<Sampler>& GetSampler() const { return m_Sampler; }
        void SetSampler(const Ref<Sampler>& sampler);

        virtual bool operator==(const Image& other) const final
        {
            return m_UUID == other.m_UUID;
        }

        Image& operator=(const Image&) = delete;
        Image& operator=(Image&&) = delete;

        static Ref<Image> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );

        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );

      protected:
        Image(const GraphicsContext* context, const SImageCreateInfo& info);
        Image(const Image&) = delete;
        Image(Image&&) = delete;

        virtual void UpdateSampler() = 0;

        UUID m_UUID;

        EImageType m_Type;
        EImageLayout m_Layout;
        EImageFormat m_Format;
        EImageUsage m_Usage;
        EImageAspect m_Aspect;

        Extent3D m_Extent;

        uint32_t m_MipLevels;
        uint32_t m_ArrayLayers;

        uint32_t m_BitsPerPixel;
        size_t m_Size;

        bool m_HDR = false;

        Ref<Sampler> m_Sampler;

        const GraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API DepthStencilImage : public Image
    {
      public:
        ~DepthStencilImage() override = default;

        [[nodiscard]] virtual uint32_t GetHeight() const { return m_Extent.Height; }

        static Ref<DepthStencilImage> Create(
            const GraphicsContext* context,
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );

        static SImageCreateInfo CreateImageInfo(
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );

      protected:
        DepthStencilImage(const GraphicsContext* context, const SImageCreateInfo& info);
    };
}