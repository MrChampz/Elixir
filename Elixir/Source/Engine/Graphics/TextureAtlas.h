#pragma once

#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    /** Describes equally sized frames in a row-major texture grid. */
    struct STextureAtlasLayout
    {
        /** Number of horizontal cells. */
        uint32_t Columns = 1;

        /** Number of vertical cells. */
        uint32_t Rows = 1;

        /** Number of usable cells, starting at the top-left corner. */
        uint32_t Frames = 1;

        /** @brief Checks dimensions and the frame range supported by shader floats. */
        bool IsValid() const
        {
            const uint64_t cells = uint64_t(Columns) * Rows;
            return Columns > 0 && Rows > 0 &&
                cells <= 16'777'216 &&
                Frames > 0 && Frames <= cells;
        }

        bool operator==(const STextureAtlasLayout& other) const = default;
    };

    /**
     * @brief Owns a shared texture and its immutable frame layout.
     *
     * Sampling and animation policies belong to materials that use this resource.
     */
    class ELIXIR_API TextureAtlas final
    {
      public:
        /**
         * @brief Creates an atlas over a valid two-dimensional texture.
         * @param texture Texture retained by the atlas.
         * @param layout Grid of equally sized UV regions. Each cell must span at least one
         * texel.
         * @return New resource, or null when the texture or layout is invalid.
         */
        static Ref<TextureAtlas> Create(
            const Ref<Texture>& texture,
            STextureAtlasLayout layout
        );

        /** @brief Returns this resource's identity, independent of its texture. */
        const UUID& GetUUID() const { return m_UUID; }

        /** @brief Returns the texture retained by this atlas. */
        const Ref<Texture>& GetTexture() const { return m_Texture; }

        /** @brief Returns the immutable frame layout. */
        const STextureAtlasLayout& GetLayout() const { return m_Layout; }

        /**
         * @brief Prevents copying a resource identity.
         * @param other Resource that cannot be copied.
         */
        TextureAtlas(const TextureAtlas& other) = delete;

        /**
         * @brief Prevents replacing a resource identity by assignment.
         * @param other Resource that cannot be assigned.
         */
        TextureAtlas& operator=(const TextureAtlas& other) = delete;

      private:
        /**
         * @brief Retains a validated texture and layout.
         * @param texture Texture retained by the atlas.
         * @param layout Validated frame layout.
         */
        TextureAtlas(Ref<Texture> texture, STextureAtlasLayout layout);

        UUID m_UUID;
        Ref<Texture> m_Texture;
        STextureAtlasLayout m_Layout;
    };
}
