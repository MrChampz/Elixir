#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/GraphicsContext.h>

namespace Elixir
{
    class CommandBuffer;
    struct SBufferImageCopy;

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

    class ELIXIR_API Image
    {
        friend class TextureLoader;
    public:
        virtual ~Image() = default;

        // TODO: secure pointer and raw pointer versions needed?
        virtual void Transition(const Ref<CommandBuffer>& cmd, EImageLayout layout);
        virtual void Transition(const CommandBuffer* cmd, EImageLayout layout) = 0;

        // TODO: secure pointer and raw pointer versions needed?
        virtual void Copy(const Ref<CommandBuffer>& cmd, const Ref<Image>& dst);
        virtual void Copy(
            const Ref<CommandBuffer>& cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );
        virtual void Copy(const CommandBuffer* cmd, const Ref<Image>& dst);
        virtual void Copy(
            const CommandBuffer* cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        ) = 0;

        // virtual void CopyFromBuffer(
        //     const Ref<CommandBuffer>& cmd,
        //     const Ref<BufferBase>& src,
        //     std::span<BufferImageCopy> regions = {}
        // );

        [[nodiscard]] virtual Extent3D GetExtent() const
        {
            return { m_Width, 1, 1};
        }

        [[nodiscard]] const UUID& GetUUID() const { return m_UUID; }

        [[nodiscard]] EImageType GetType() const { return m_Type; }
        [[nodiscard]] EImageLayout GetLayout() const { return m_Layout; }
        [[nodiscard]] EImageFormat GetFormat() const { return m_Format; }
        [[nodiscard]] EImageUsage GetUsage() const { return m_Usage; }
        [[nodiscard]] EImageAspect GetAspect() const { return m_Aspect; }

        [[nodiscard]] uint32_t GetWidth() const { return m_Width; }

        [[nodiscard]] uint32_t GetBitsPerPixel() const { return m_BitsPerPixel; }
        [[nodiscard]] size_t GetSize() const { return m_Size; }

        [[nodiscard]] bool IsHDR() const { return m_IsHDR; }

        // [[nodiscard]] Ref<Sampler> GetSampler() const { return m_Sampler; }
        // void SetSampler(const Ref<Sampler>& sampler); # call UpdateSampler!

        virtual bool operator==(const Image& other) const final
        {
            return m_UUID == other.m_UUID;
        }

        static Ref<Image> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );

      protected:
        Image(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );

        virtual void CreateImage(void* data) = 0;
        virtual void UpdateSampler() = 0;

        UUID m_UUID;

        EImageType m_Type;
        EImageLayout m_Layout;
        EImageFormat m_Format;
        EImageUsage m_Usage;
        EImageAspect m_Aspect;

        uint32_t m_Width;

        uint32_t m_BitsPerPixel;
        size_t m_Size;

        bool m_IsHDR = false;

        //Ref<Sampler> m_Sampler;

        const GraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API DepthStencilImage : public virtual Image
    {
      public:
        ~DepthStencilImage() override = default;

        [[nodiscard]] Extent3D GetExtent() const override
        {
            return { m_Width, m_Height, 1 };
        }

        [[nodiscard]] virtual uint32_t GetHeight() const { return m_Height; }

        static Ref<DepthStencilImage> Create(
            const GraphicsContext* context,
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );

      protected:
        DepthStencilImage(
            const GraphicsContext* context,
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );

      private:
        uint32_t m_Height;
    };

    class ELIXIR_API StorageImage : public virtual Image
    {
      public:
        ~StorageImage() override = default;

        [[nodiscard]] Extent3D GetExtent() const override
        {
            return { m_Width, m_Height, 1 };
        }

        [[nodiscard]] virtual uint32_t GetHeight() const { return m_Height; }

        static Ref<StorageImage> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height
        );

      protected:
        StorageImage(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height
        );

        uint32_t m_Height;
    };
}