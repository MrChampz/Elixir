#pragma once

#include <Engine/Graphics/Definitions.h>
#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Shader/ShaderBinding.h>

namespace Elixir
{
    class ELIXIR_API TextureSet
    {
      public:
        virtual ~TextureSet() = default;

        /**
         * Clear all textures
         */
        void Clear();

        /**
         * Add a texture to the set and get its handle.
         * @param texture Reference to the texture
         * @return The texture handle in the set
         */
        virtual SResourceHandle AddTexture(const Ref<Texture>& texture) = 0;

        /**
         * Remove a texture by its handle.
         * @param handle Texture handle
         */
        virtual void RemoveTexture(SResourceHandle handle) = 0;

        /**
         * Get the number of textures currently in the set.
         * @return The number of textures currently in the set.
         */
        uint32_t GetTextureCount() const { return m_TextureCount; }

        static Ref<TextureSet> Create(const GraphicsContext* context);

      protected:
        explicit TextureSet(const GraphicsContext* context);

        uint32_t m_TextureCount = 0;
        std::unordered_map<SResourceHandle, Ref<Texture>> m_Textures;

        const GraphicsContext* m_GraphicsContext;
    };
}