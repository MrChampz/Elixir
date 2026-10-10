#pragma once

#include <Engine/Graphics/Image.h>

namespace Elixir
{
    /** @brief Defines image properties and asset metadata shared by texture dimensions. */
    struct STextureCreateInfo
    {
        /** Pixel format of the texture. */
        EImageFormat Format = EImageFormat::Undefined;
        
        /** Base-level width in texels. */
        uint32_t Width = 0;
        
        /** Number of supplied mip levels when using LeaveExistingMips. */
        uint32_t MipLevels = 1;
        
        /** Mip policy forwarded to Image without resolving it. */
        EImageMipmapMode MipmapMode = EImageMipmapMode::NoMipmaps;
        
        /** Source path retained for asset tracking. */
        std::string Path;
        
        /** Records whether the source pixels were decoded as HDR data. */
        bool HDR = false;
        
        /** Base-level pixels borrowed until creation returns. */
        const void* InitialData = nullptr;
        
        /** Complete existing mip chain; mutually exclusive with InitialData. */
        std::span<const SImageMipData> InitialMipData;
    };

    /** @brief Defines a two-dimensional texture asset. */
    struct STexture2DCreateInfo : STextureCreateInfo
    {
        /** Base-level height in texels. */
        uint32_t Height = 0;
    };

    /** @brief Defines a three-dimensional texture asset. */
    struct STexture3DCreateInfo : STexture2DCreateInfo
    {
        /** Base-level depth in texels. */
        uint32_t Depth = 0;
    };

    /**
     * @brief Owns asset metadata and a shared image resource, independently of the
     * graphics backend.
     */
    class ELIXIR_API Texture
    {
    public:
        /** @brief Releases the asset's reference to its image. */
        virtual ~Texture() = default;

        Texture(const Texture&) = delete;
        Texture(Texture&&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture& operator=(Texture&&) = delete;

        /** @brief Returns the image used for rendering and resource operations. */
        const Ref<Image>& GetImage() const { return m_Image; }

        /** @brief Returns this asset's identity, independent of the image resource. */
        const UUID& GetUUID() const { return m_UUID; }

        /** @brief Returns the source path, or an empty string for procedural assets. */
        const std::string& GetPath() const { return m_Path; }

        /** @brief Reports whether the source was decoded as HDR data. */
        bool IsHDR() const { return m_HDR; }

        /** @brief Reports whether the image has allocated storage. */
        bool IsValid() const { return m_Image && m_Image->IsValid(); }

        /** @brief Returns the image dimension. */
        EImageType GetType() const { return m_Image->GetType(); }

        /** @brief Returns the image pixel format. */
        EImageFormat GetFormat() const { return m_Image->GetFormat(); }

        /** @brief Returns the base-level dimensions in texels. */
        Extent3D GetExtent() const { return m_Image->GetExtent(); }

        /** @brief Returns the base-level width in texels. */
        uint32_t GetWidth() const { return m_Image->GetWidth(); }

        /** @brief Returns the base-level height in texels. */
        uint32_t GetHeight() const { return m_Image->GetHeight(); }

        /** @brief Returns the base-level depth in texels. */
        uint32_t GetDepth() const { return m_Image->GetDepth(); }

        /** @brief Returns the allocated mip-level count. */
        uint32_t GetMipLevels() const { return m_Image->GetMipLevels(); }

        /** @brief Returns the base-level storage size in bytes. */
        size_t GetSize() const { return m_Image->GetSize(); }

        /**
         * @brief Creates a one-dimensional texture and delegates image initialization to
         * Image.
         * @param context Graphics context that must outlive the texture's image.
         * @param info Texture properties, pixel data, mip policy, and asset metadata.
         * @return The initialized texture, or null when image creation fails.
         */
        static Ref<Texture> Create(
            const GraphicsContext* context,
            const STextureCreateInfo& info
        );
        
        /**
         * @brief Creates a one-dimensional texture with a single mip level.
         * @param context Graphics context that must outlive the texture's image.
         * @param format Pixel format of the image and supplied data.
         * @param width Base-level width in texels; must be greater than zero.
         * @param data Optional base-level pixels, borrowed until creation returns.
         * @param path Source path retained for asset tracking; empty for procedural assets.
         */
        static Ref<Texture> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr,
            const std::string& path = ""
        );
        
        /**
         * @brief Maps texture properties to an image request without resolving mip levels or
         * upload usage.
         * @param info Texture properties and borrowed pixel data to forward to Image.
         */
        static SImageCreateInfo CreateImageInfo(const STextureCreateInfo& info);
        
        /**
         * @brief Builds an unresolved one-dimensional image request.
         * @param format Pixel format of the image and supplied data.
         * @param width Base-level width in texels; must be greater than zero.
         * @param data Optional base-level pixels that must remain valid until
         * Image::Create returns.
         * @param path Source path; not included in the returned image description.
         */
        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr,
            const std::string& path = ""
        );

    protected:
        Texture(Ref<Image> image, const STextureCreateInfo& info);

    private:
        UUID m_UUID;
        Ref<Image> m_Image;
        std::string m_Path;
        bool m_HDR;
    };

    /** @brief Wraps a two-dimensional image as a texture asset. */
    class ELIXIR_API Texture2D final : public Texture
    {
    public:
        /**
         * @brief Creates a two-dimensional asset and initializes its image.
         * @param context Graphics context that must outlive the texture's image.
         * @param info Texture properties, pixel data, mip policy, and asset metadata.
         * @return The initialized texture, or null when image creation fails.
         */
        static Ref<Texture2D> Create(
            const GraphicsContext* context,
            const STexture2DCreateInfo& info
        );

        /**
         * @brief Creates a two-dimensional asset with a single mip level.
         * @param context Graphics context that must outlive the texture's image.
         * @param format Pixel format of the image and supplied data.
         * @param width Base-level width in texels; must be greater than zero.
         * @param height Base-level height in texels; must be greater than zero.
         * @param data Optional base-level pixels, borrowed until creation returns.
         * @param path Source path retained for asset tracking; empty for procedural assets.
         */
        static Ref<Texture2D> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            const void* data = nullptr,
            const std::string& path = ""
        );

        /**
         * @brief Maps texture properties to an unresolved two-dimensional image request.
         * @param info Texture properties and borrowed pixel data to forward to Image.
         */
        static SImageCreateInfo CreateImageInfo(const STexture2DCreateInfo& info);

        /**
         * @brief Builds an unresolved two-dimensional image request.
         * @param format Pixel format of the image and supplied data.
         * @param width Base-level width in texels; must be greater than zero.
         * @param height Base-level height in texels; must be greater than zero.
         * @param data Optional base-level pixels that must remain valid until
         * Image::Create returns.
         */
        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            const void* data = nullptr
        );

    private:
        // Retains the initialized image and its asset metadata.
        Texture2D(Ref<Image> image, const STextureCreateInfo& info);
    };

    /** @brief Wraps a three-dimensional image as a texture asset. */
    class ELIXIR_API Texture3D final : public Texture
    {
    public:
        /**
         * @brief Creates a three-dimensional asset and initializes its image.
         * @param context Graphics context that must outlive the texture's image.
         * @param info Texture properties, pixel data, mip policy, and asset metadata.
         * @return The initialized texture, or null when image creation fails.
         */
        static Ref<Texture3D> Create(
            const GraphicsContext* context,
            const STexture3DCreateInfo& info
        );

        /**
         * @brief Creates a three-dimensional asset with a single mip level.
         * @param context Graphics context that must outlive the texture's image.
         * @param format Pixel format of the image and supplied data.
         * @param width Base-level width in texels; must be greater than zero.
         * @param height Base-level height in texels; must be greater than zero.
         * @param depth Base-level depth in texels; must be greater than zero.
         * @param data Optional base-level pixels, borrowed until creation returns.
         * @param path Source path retained for asset tracking; empty for procedural assets.
         */
        static Ref<Texture3D> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            const void* data = nullptr,
            const std::string& path = ""
        );

        /**
         * @brief Maps texture properties to an unresolved three-dimensional image request.
         * @param info Texture properties and borrowed pixel data to forward to Image.
         */
        static SImageCreateInfo CreateImageInfo(const STexture3DCreateInfo& info);

        /**
         * @brief Builds an unresolved three-dimensional image request.
         * @param format Pixel format of the image and supplied data.
         * @param width Base-level width in texels; must be greater than zero.
         * @param height Base-level height in texels; must be greater than zero.
         * @param depth Base-level depth in texels; must be greater than zero.
         * @param data Optional base-level pixels that must remain valid until
         * Image::Create returns.
         */
        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            const void* data = nullptr
        );

    private:
        // Retains the initialized image and its asset metadata.
        Texture3D(Ref<Image> image, const STextureCreateInfo& info);
    };
}
