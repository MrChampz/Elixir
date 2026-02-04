#pragma once
#include "Texture.h"

namespace Elixir
{
    class ELIXIR_API TextureSet
    {
      public:
        static constexpr uint32_t MAX_TEXTURES = 1024;

        virtual ~TextureSet() = default;

        // Disable copy, enable move
        TextureSet(const TextureSet&) = delete;
        TextureSet& operator=(const TextureSet&) = delete;
        TextureSet(TextureSet&&) noexcept;
        TextureSet& operator=(TextureSet&&) noexcept;

        /**
         * Clear all textures
         */
        void Clear();

        /**
         * Add a texture to the set and get its index.
         * @param texture Reference to the texture
         * @return The texture index in the set
         */
        uint32_t AddTexture(const Ref<Texture>& texture);

        /**
         * Add a texture at a specific index (overwrites if already exists).
         * @param index Texture index
         * @param texture Reference to the texture
         */
        virtual void SetTexture(uint32_t index, const Ref<Texture>& texture);

        /**
         * Remove a texture by index (frees the slot for reuse).
         * @param index Texture index
         */
        virtual void RemoveTexture(uint32_t index);

        /**
         * Check if a slot is occupied.
         * @param index Texture index
         * @return True if a texture exists at the specified index
         */
        bool HasTexture(uint32_t index) const;

        /**
         * Get the texture at a specific index
         * @param index Texture index
         * @return Reference to the texture
         */
        Ref<Texture> GetTexture(uint32_t index) const;

        /**
         * Get the list of textures in the set.
         * @return The list of textures in the set.
         */
        const std::vector<Ref<Texture>>& GetTextures() const { return m_Textures; }

        /**
         * Get the index of a specific texture in the set.
         * @param texture Reference to the texture
         * @return The texture index in the set.
         */
        uint32_t GetTextureIndex(const Ref<Texture>& texture) const;

        /**
         * Get the number of textures currently in the set.
         * @return The number of textures currently in the set.
         */
        uint32_t GetTextureCount() const { return m_TextureCount; }

        /**
         * Get the maximum capacity.
         * @return The maximum number of textures supported by the set.
         */
        uint32_t GetMaxTextures() const { return m_MaxTextures; }

        static Ref<TextureSet> Create(
            const GraphicsContext* context,
            uint32_t maxTextures = MAX_TEXTURES
        );

      protected:
        explicit TextureSet(
            const GraphicsContext* context,
            uint32_t maxTextures = MAX_TEXTURES
        );

        uint32_t FindFreeSlot() const;

        uint32_t m_MaxTextures;
        uint32_t m_TextureCount = 0;
        std::vector<Ref<Texture>> m_Textures;
        std::vector<uint32_t> m_FreeSlots;
        bool m_NeedsUpdate = false;

        const GraphicsContext* m_GraphicsContext;
    };
}