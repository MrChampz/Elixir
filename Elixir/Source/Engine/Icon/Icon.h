#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    class GraphicsContext;
}

namespace Elixir
{
    /** @brief Identifies the source format of an icon asset. */
    enum class EIconFormat : uint8_t
    {
        SVG,
        PNG,
        WebP,
    };

    /** @brief Describes an icon file before a format loader imports it. */
    struct SIconSource
    {
        std::filesystem::path Path;
        EIconFormat Format;
    };

    /** @brief Describes the intrinsic size of imported icon content. */
    struct SIconMetrics
    {
        glm::vec2 Size{ 1.0f, 1.0f };
    };

    /** @brief Requests one rasterized icon size in physical pixels. */
    struct SIconRasterRequest
    {
        glm::uvec2 PixelSize{ 1, 1 };
    };

    /** @brief Stores pixels produced by an icon content implementation. */
    struct SIconBitmap
    {
        glm::uvec2 Size{};
        std::vector<uint8_t> Pixels;

        /** @brief Check whether this bitmap contains one RGBA pixel buffer. */
        bool IsValid() const
        {
            return Size.x > 0 && Size.y > 0 &&
                   Pixels.size() == size_t(Size.x) * Size.y * 4;
        }
    };

    /**
     * @brief Represents imported icon data without exposing its source library.
     *
     * A concrete content type owns the native data returned by its loader. It rasterizes on
     * demand, which lets SVG, raster files and future compiled formats share Icon.
     */
    class ELIXIR_API IconContent
    {
      public:
        virtual ~IconContent() = default;

        /** @brief Get the icon's intrinsic size. */
        virtual SIconMetrics GetMetrics() const = 0;

        /**
         * @brief Rasterize this icon at one physical size.
         * @param request Requested output dimensions.
         * @return RGBA bitmap, or an invalid bitmap when rasterization fails.
         */
        virtual SIconBitmap Rasterize(const SIconRasterRequest& request) const = 0;
    };

    /**
     * @brief Imports one icon format into opaque icon content.
     *
     * Loaders do not leak parser types through engine headers. Register another loader to add a
     * format or replace the implementation that imports an existing format.
     */
    class ELIXIR_API IconLoader
    {
      public:
        virtual ~IconLoader() = default;

        /** @brief Get the format this loader imports. */
        virtual EIconFormat GetFormat() const = 0;

        /**
         * @brief Import one source file.
         * @param source File and format to import.
         * @return Imported content, or nullptr when the source is invalid.
         */
        virtual Ref<IconContent> Load(const SIconSource& source) const = 0;
    };

    /**
     * @brief Identifies one imported icon and caches its GPU rasters.
     *
     * An asset is format-neutral. It delegates rasterization to its opaque IconContent and
     * stores one texture for each requested physical size.
     */
    class ELIXIR_API Icon final
    {
        friend class IconManager;

      public:
        /** @brief Get the path used to import this icon. */
        const std::filesystem::path& GetPath() const { return m_Source.Path; }

        /** @brief Get this asset's source format. */
        EIconFormat GetFormat() const { return m_Source.Format; }

        /** @brief Get the icon's intrinsic size. */
        SIconMetrics GetMetrics() const { return m_Content->GetMetrics(); }

        /**
         * @brief Get a raster texture for one logical render size.
         * @param logicalSize Icon dimensions in logical render units.
         * @return Cached or newly rasterized texture, or nullptr when rasterization fails.
         */
        Ref<Texture2D> GetTexture(const glm::vec2& logicalSize) const;

      private:
        Icon(
            const GraphicsContext* context,
            SIconSource source,
            Ref<IconContent> content
        );

        const GraphicsContext* m_GraphicsContext = nullptr;
        SIconSource m_Source;
        Ref<IconContent> m_Content;
        mutable std::unordered_map<uint64_t, Ref<Texture2D>> m_Textures;
    };
}
