#pragma once

#include <Engine/Graphics/Image.h>

namespace Elixir
{
    /** @brief Defines properties shared by every texture dimension. */
    struct STextureCreateInfo
    {
        /** Pixel data for the base mip level, or null for an empty texture. */
        const void* InitialData = nullptr;

        /** Pixel format of the texture. */
        EImageFormat Format = EImageFormat::Undefined;

        /** Width of the base mip level in texels. */
        uint32_t Width = 0;

        /** Number of mip levels allocated for the texture. */
        uint32_t MipLevels = 1;

        /** Generates lower mip levels from the base level during upload. */
        bool GenerateMipmaps = false;

        /** Source path retained for diagnostics and asset tracking. */
        std::string Path;
    };

    /** @brief Defines properties for a two-dimensional texture. */
    struct STexture2DCreateInfo : STextureCreateInfo
    {
        /** Height of the base mip level in texels. */
        uint32_t Height = 0;
    };

    /** @brief Defines properties for a three-dimensional texture. */
    struct STexture3DCreateInfo : STexture2DCreateInfo
    {
        /** Depth of the base mip level in texels. */
        uint32_t Depth = 0;
    };

    class ELIXIR_API Texture : public Image
    {
      public:
        ~Texture() override = default;

        [[nodiscard]] const std::string& GetPath() const { return m_Path; }

        /** @brief Creates a one-dimensional texture from the supplied description. */
        static Ref<Texture> Create(
            const GraphicsContext* context,
            const STextureCreateInfo& info
        );

        static Ref<Texture> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr,
            const std::string& path = ""
        );

        /** @brief Builds the image description used to create a one-dimensional texture. */
        static SImageCreateInfo CreateImageInfo(
            const STextureCreateInfo& info
        );

        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr,
            const std::string& path = ""
        );

      protected:
        Texture(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            const void* data = nullptr,
            const std::string& path = ""
        );
        Texture(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );

        std::string m_Path;
    };

    class ELIXIR_API Texture2D : public Texture
    {
      public:
        ~Texture2D() override = default;

        [[nodiscard]] uint32_t GetHeight() const { return m_Extent.Height; }

        /** @brief Creates a two-dimensional texture from the supplied description. */
        static Ref<Texture2D> Create(
            const GraphicsContext* context,
            const STexture2DCreateInfo& info
        );

        static Ref<Texture2D> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            const void* data = nullptr,
            const std::string& path = ""
        );

        /** @brief Builds the image description used to create a two-dimensional texture. */
        static SImageCreateInfo CreateImageInfo(
            const STexture2DCreateInfo& info
        );

        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            const void* data = nullptr
        );

      protected:
        Texture2D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            const void* data = nullptr,
            const std::string& path = ""
        );
        Texture2D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };

    class ELIXIR_API Texture3D : public Texture2D
    {
    public:
        ~Texture3D() override = default;

        [[nodiscard]] uint32_t GetDepth() const { return m_Extent.Depth; }

        /** @brief Creates a three-dimensional texture from the supplied description. */
        static Ref<Texture3D> Create(
            const GraphicsContext* context,
            const STexture3DCreateInfo& info
        );

        static Ref<Texture3D> Create(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            const void* data = nullptr,
            const std::string& path = ""
        );

        /** @brief Builds the image description used to create a three-dimensional texture. */
        static SImageCreateInfo CreateImageInfo(
            const STexture3DCreateInfo& info
        );

        static SImageCreateInfo CreateImageInfo(
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            const void* data = nullptr
        );

    protected:
        Texture3D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            const void* data = nullptr,
            const std::string& path = ""
        );
        Texture3D(
            const GraphicsContext* context,
            const SImageCreateInfo& info,
            const std::string& path = ""
        );
    };
}
