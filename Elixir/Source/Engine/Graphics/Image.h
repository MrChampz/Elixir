#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/Buffer.h>

#include <mutex>
#include <optional>
#include <span>

namespace Elixir
{
    class Image;

    namespace Vulkan { class VulkanGraphicsContext; }

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

    /** @brief Defines how an image receives its mip chain. */
    enum class EImageMipmapMode
    {
        /** Keeps only the base mip level. */
        NoMipmaps,

        /** Generates lower mip levels by averaging pixel values. */
        SimpleAverage,

        /** Generates lower mip levels by averaging and normalizing normal vectors. */
        NormalMap,

        /** Preserves the mip levels supplied by the image source. */
        LeaveExistingMips
    };

	enum class EImageFormat
	{
	    Undefined = 1,
	    R8_UNORM,
        R8_SNORM,
        R8_USCALED,
        R8_SSCALED,
        R8_UINT,
        R8_SINT,
        R8_SRGB,
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
		X8_D24_UNORM_PACK32 = 504
	};

	enum class EDepthStencilImageFormat
	{
	    Undefined = 1,
		D16_UNORM = 500,
		D24_UNORM_S8_UINT = 501,
		D32_SFLOAT = 502,
		D32_SFLOAT_S8_UINT = 503,
		X8_D24_UNORM_PACK32 = 504,
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

        /** @param extent Dimensions of the image region to copy, in texels. */
        static SBufferImageCopy Default(const Extent3D& extent = Extent3D())
        {
            return {
                .ImageSubresource = SImageLayeredSubresource::Default(),
                .ImageExtent = extent,
            };
        }
    };

    /** @brief Supplies tightly packed pixels for one mip level and array layer. */
    struct SImageMipData
    {
        /** Pixel storage borrowed until Image::Create returns. */
        const void* Data = nullptr;

        /** Size of the subresource in bytes, including compressed blocks. */
        size_t Size = 0;

        /** Zero-based mip level. */
        uint32_t MipLevel = 0;

        /** Zero-based array layer; use zero for volume images. */
        uint32_t ArrayLayer = 0;
    };

    /** @brief Describes image storage and optional synchronous initialization. */
    struct SImageCreateInfo
    {
        /** Base-level width in texels; must be greater than zero. */
        uint32_t Width = 0;

        /** Base-level height in texels; must be one for a 1D image. */
        uint32_t Height = 1;

        /** Base-level depth in texels; must be one unless Type is _3D. */
        uint32_t Depth = 1;

        EImageType Type = EImageType::_2D;

        EImageFormat Format = EImageFormat::Undefined;

        /** Defines how the image receives its mip chain. */
        EImageMipmapMode MipmapMode = EImageMipmapMode::NoMipmaps;

        /** Used only by LeaveExistingMips; other modes determine the count. */
        uint32_t MipLevels = 1;

        /** Number of array layers; volume images require one layer. */
        uint32_t ArrayLayers = 1;

        /** Requested uses; Image adds the transfer flags needed for initialization. */
        EImageUsage Usage = EImageUsage::Sampled;

        /** Layout after initialization; Undefined with initial pixels resolves to General. */
        EImageLayout InitialLayout = EImageLayout::Undefined;

        /** Memory properties requested from the backend allocator. */
        SAllocationInfo AllocationInfo{ .RequiredFlags = EMemoryProperty::DeviceLocal };

        /**
         * Tightly packed base-level pixels for all layers; mutually exclusive with
         * InitialMipData.
         */
        const void* InitialData = nullptr;

        /** Complete supplied mip chain, or empty to allocate without uploading it. */
        std::span<const SImageMipData> InitialMipData;
    };

    /**
     * @brief Owns a graphics image and coordinates initialization independently of its
     * backend.
     */
    class ELIXIR_API Image : public std::enable_shared_from_this<Image>
    {
      public:
        virtual ~Image() = default;

        /**
         * @brief Releases storage after all GPU work using the image has completed; repeated
         * calls are safe.
         */
        virtual void Destroy() = 0;

        /**
         * @brief Queues an image resize and returns without waiting when called off the rendering thread.
         * @param extent New base-level dimensions in texels.
         */
        void Resize(const Extent3D& extent);

        /**
         * @brief Resizes the image and waits until the operation completes.
         * @param extent New base-level dimensions in texels.
         * @return True when the resize task was accepted by the graphics context.
         */
        bool ResizeAndWait(const Extent3D& extent);

        /**
         * @brief Records a layout transition for all mip levels and layers.
         * @param cmd Recording command buffer that receives the transition.
         * @param layout Target layout for all mip levels and array layers.
         */
        void Transition(const Ref<CommandBuffer>& cmd, EImageLayout layout);

        /**
         * @brief Records a layout transition for all mip levels and layers.
         * @param cmd Recording command buffer that receives the transition.
         * @param layout Target layout for all mip levels and array layers.
         */
        virtual void Transition(const CommandBuffer* cmd, EImageLayout layout) = 0;

        /**
         * @brief Blits the base level into dst.
         *
         * Images must already use transfer layouts and usages.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image whose base level receives the scaled source pixels.
         */
        void Copy(const Ref<CommandBuffer>& cmd, const Ref<Image>& dst);

        /**
         * @brief Blits the base level into dst.
         *
         * Images must already use transfer layouts and usages.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image whose base level receives the scaled source pixels.
         */
        void Copy(const Ref<CommandBuffer>& cmd, Image* dst);

        /**
         * @brief Blits the base level into dst.
         *
         * Images must already use transfer layouts and usages.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image whose base level receives the scaled source pixels.
         */
        void Copy(const CommandBuffer* cmd, const Ref<Image>& dst);

        /**
         * @brief Blits the base level into dst.
         *
         * Images must already use transfer layouts and usages.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image whose base level receives the scaled source pixels.
         */
        void Copy(const CommandBuffer* cmd, Image* dst);

        /**
         * @brief Blits between base-level extents without resizing either image.
         *
         * Transfer layouts are required.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image that receives the scaled source pixels.
         * @param srcExtent Source region dimensions in texels, measured from the base-level
         * origin.
         * @param dstExtent Destination region dimensions in texels, measured from the
         * base-level origin.
         */
        void Copy(
            const Ref<CommandBuffer>& cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );

        /**
         * @brief Blits between base-level extents without resizing either image.
         *
         * Transfer layouts are required.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image that receives the scaled source pixels.
         * @param srcExtent Source region dimensions in texels, measured from the base-level
         * origin.
         * @param dstExtent Destination region dimensions in texels, measured from the
         * base-level origin.
         */
        void Copy(
            const Ref<CommandBuffer>& cmd,
            Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );

        /**
         * @brief Blits between base-level extents without resizing either image.
         *
         * Transfer layouts are required.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image that receives the scaled source pixels.
         * @param srcExtent Source region dimensions in texels, measured from the base-level
         * origin.
         * @param dstExtent Destination region dimensions in texels, measured from the
         * base-level origin.
         */
        void Copy(
            const CommandBuffer* cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );

        /**
         * @brief Blits between base-level extents without resizing either image.
         *
         * Transfer layouts are required.
         *
         * @param cmd Recording command buffer that receives the blit.
         * @param dst Destination image that receives the scaled source pixels.
         * @param srcExtent Source region dimensions in texels, measured from the base-level
         * origin.
         * @param dstExtent Destination region dimensions in texels, measured from the
         * base-level origin.
         */
        void Copy(
            const CommandBuffer* cmd,
            Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        );

        /**
         * @brief Records a buffer upload into TransferDst storage.
         *
         * An empty region list selects the base level.
         *
         * @param cmd Recording command buffer that receives the upload.
         * @param src Source buffer containing the pixel data.
         * @param regions Buffer-to-image copy regions; empty selects the full base level of
         * every array layer.
         */
        void CopyFrom(
            const Ref<CommandBuffer>& cmd,
            const Ref<Buffer>& src,
            std::span<SBufferImageCopy> regions = {}
        );

        /**
         * @brief Records a buffer upload into TransferDst storage.
         *
         * An empty region list selects the base level.
         *
         * @param cmd Recording command buffer that receives the upload.
         * @param src Source buffer containing the pixel data.
         * @param regions Buffer-to-image copy regions; empty selects the full base level of
         * every array layer.
         */
        void CopyFrom(
            const Ref<CommandBuffer>& cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        );

        /**
         * @brief Records a buffer upload into TransferDst storage.
         *
         * An empty region list selects the base level.
         *
         * @param cmd Recording command buffer that receives the upload.
         * @param src Source buffer containing the pixel data.
         * @param regions Buffer-to-image copy regions; empty selects the full base level of
         * every array layer.
         */
        void CopyFrom(
            const CommandBuffer* cmd,
            const Ref<Buffer>& src,
            std::span<SBufferImageCopy> regions = {}
        );

        /**
         * @brief Records a buffer upload into TransferDst storage.
         *
         * An empty region list selects the base level.
         *
         * @param cmd Recording command buffer that receives the upload.
         * @param src Source buffer containing the pixel data.
         * @param regions Buffer-to-image copy regions; empty selects the full base level of
         * every array layer.
         */
        virtual void CopyFrom(
            const CommandBuffer* cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        ) = 0;

        /** @brief Reports whether this image is valid. */
        virtual bool IsValid() const = 0;

        /** @brief Returns the identity of this resource. */
        const UUID& GetUUID() const { return m_UUID; }

        /** @brief Returns the dimensionality of the image. */
        EImageType GetType() const { return m_Type; }

        /** @brief Returns the layout tracked for all mip levels and layers. */
        EImageLayout GetLayout() const { return m_Layout; }

        /** @brief Returns the pixel or compressed-block format. */
        EImageFormat GetFormat() const { return m_Format; }

        /** @brief Returns the resolved usage flags, including upload requirements. */
        EImageUsage GetUsage() const { return m_Usage; }

        /** @brief Returns the color, depth, or stencil aspects stored by the image. */
        EImageAspect GetAspect() const { return m_Aspect; }

        /** @brief Returns the base-level dimensions in texels. */
        Extent3D GetExtent() const { return m_Extent; }

        /** @brief Returns the base-level width in texels. */
        uint32_t GetWidth() const { return m_Extent.Width; }

        /** @brief Returns the base-level height in texels. */
        uint32_t GetHeight() const { return m_Extent.Height; }

        /** @brief Returns the base-level depth in texels. */
        uint32_t GetDepth() const { return m_Extent.Depth; }

        /** @brief Returns the extent of an allocated mip level.
         * @param level Zero-based mip level, less than GetMipLevels().
         */
        Extent3D GetMipExtent(uint32_t level) const;

        /** @brief Returns tightly packed bytes for one array layer of an allocated mip level.
         * @param level Zero-based mip level, less than GetMipLevels().
         */
        size_t GetMipSize(uint32_t level) const;

        /** @brief Returns the allocated mip-level count. */
        uint32_t GetMipLevels() const { return m_MipLevels; }

        /** @brief Returns the allocated array-layer count. */
        uint32_t GetArrayLayers() const { return m_ArrayLayers; }

        /** @brief Returns bits per texel, averaged across a block for compressed formats. */
        uint32_t GetBitsPerPixel() const { return m_BitsPerPixel; }

        /** @brief Returns whole bytes per texel; use GetMipSize for compressed storage sizes. */
        uint32_t GetBytesPerPixel() const { return m_BitsPerPixel / CHAR_BIT; }

        /**
         * @brief Returns the resolved allocation description without borrowed upload data
         * or generation requests.
         */
        SImageCreateInfo GetCreateInfo() const;

        /**
         * @brief Returns tightly packed base-level bytes across all layers, including
         * compressed blocks.
         */
        size_t GetSize() const { return m_Size; }

        bool operator==(const Image& other) const
        {
            return m_UUID == other.m_UUID;
        }

        Image& operator=(const Image&) = delete;
        Image& operator=(Image&&) = delete;

        /**
         * @brief Resolves the description, creates the backend, uploads data, and prepares
         * the final layout.
         *
         * Upload storage is borrowed only for this call.
         *
         * @pre The context outlives the image, and no upload command buffer is recording on
         * this thread.
         * @param context Graphics context used to create the image; must outlive it.
         * @param info Image description; supplied pixel storage must remain valid until this
         * call returns.
         */
        static Ref<Image> Create(const GraphicsContext* context, SImageCreateInfo info);

        /**
         * @brief Creates a one-dimensional sampled image.
         * @param context Graphics context that must outlive the image.
         * @param format Pixel or compressed-block format.
         * @param width Base-level width in texels; must be greater than zero.
         * @param data Optional tightly packed base-level pixels, borrowed until this call
         * returns.
         */
        static Ref<Image> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr
        );

        /**
         * @brief Builds a one-dimensional sampled-image request without resolving mip levels.
         * @param format Pixel or compressed-block format.
         * @param width Base-level width in texels; must be greater than zero when creating
         * the image.
         * @param data Optional base-level pixels; the returned description borrows this
         * storage.
         */
        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr
        );

        /** @brief Returns the number of mip levels from a base image extent.
         * @param extent Base-level dimensions in texels; the largest dimension determines
         * the chain length.
         */
        static uint32_t GetFullMipLevelCount(const Extent3D& extent);

        /**
         * @brief Resolves the mip count from the requested mode and validates its range.
         * @param info Requested dimensions, mipmap mode, and explicit count for
         * LeaveExistingMips.
         */
        static uint32_t GetMipLevelCount(const SImageCreateInfo& info);

      protected:
        Image(const GraphicsContext* context, const SImageCreateInfo& info);
        Image(const Image&) = delete;
        Image(Image&&) = delete;

        /**
         * Recompute cached size fields (bits-per-pixel and byte size) from the
         * current format and extent. Call after changing the extent (e.g. on
         * resize) so GetSize() stays consistent with the actual dimensions.
         */
        void RecalculateSize();

        UUID m_UUID;
        std::string m_DebugName;

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

        SAllocationInfo m_AllocationInfo;

        const GraphicsContext* m_GraphicsContext;

        std::mutex m_ResizeMutex;
        std::optional<Extent3D> m_PendingResize;
        bool m_ResizeQueued = false;

      private:
        friend class Vulkan::VulkanGraphicsContext;

        // Creates storage and completes the requested upload after backend construction.
        void Initialize(const SImageCreateInfo& info);

        // Allocates backend resources from the already resolved description.
        virtual void CreateResource(const SImageCreateInfo& info) = 0;

        // Generates simple-average mip levels on the device and transitions the whole image.
        virtual void GenerateMipmaps(const CommandBuffer* cmd, EImageLayout finalLayout) = 0;

        // Scales one mip level across all array layers without changing either allocation.
        virtual void CopyMip(
            const CommandBuffer* cmd,
            Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent,
            uint32_t level
        ) = 0;

        // Applies queued resize requests on the rendering thread.
        void ApplyPendingResize();

        // Resizes storage and scales existing mip levels on the rendering thread.
        void ResizeOnRenderThread(const Ref<CommandBuffer>& cmd, const Extent3D& extent);
    };

}
