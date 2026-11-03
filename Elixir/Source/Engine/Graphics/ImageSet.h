#pragma once

#include <Engine/Graphics/Definitions.h>
#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Shader/ShaderBinding.h>

namespace Elixir
{
    /** @brief Retains sampled images registered in a shared descriptor array. */
    class ELIXIR_API ImageSet
    {
      public:
        /** @brief Releases the set and its image registrations. */
        virtual ~ImageSet() = default;

        /**
         * Releases all image registrations held by this set.
         */
        virtual void Clear() = 0;

        /**
         * @brief Registers a sampled image and retains it until removal or Clear.
         * @param image Reference to the image.
         * @return An invalid handle when registration fails.
         */
        virtual SResourceHandle AddImage(const Ref<Image>& image) = 0;

        /**
         * @brief Releases this set's registration of an image.
         * @param handle Image handle
         */
        virtual void RemoveImage(SResourceHandle handle) = 0;

        /**
         * Registers the image owned by a texture asset.
         * @param texture Reference to the texture.
         * @return An invalid handle when image registration fails.
         */
        virtual SResourceHandle AddTexture(const Ref<Texture>& texture);

        /**
         * Remove a texture by its handle.
         * @param handle Texture handle
         */
        virtual void RemoveTexture(const SResourceHandle handle) { RemoveImage(handle); }

        /**
         * Get the number of images currently in the set.
         * @return The number of images currently in the set.
         */
        uint32_t GetImageCount() const { return m_ImageCount; }

        /** @brief Creates an image set for the context's graphics backend.
         * @param context Graphics context that must outlive the set.
         */
        static Ref<ImageSet> Create(const GraphicsContext* context);

      protected:
        explicit ImageSet(const GraphicsContext* context);

        uint32_t m_ImageCount = 0;
        std::unordered_map<Ref<Image>, SResourceHandle> m_Images;

        const GraphicsContext* m_GraphicsContext;
    };
}
