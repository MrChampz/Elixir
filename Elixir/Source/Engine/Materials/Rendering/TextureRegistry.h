#pragma once

#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/TextureSet.h>

namespace Elixir::Materials::Rendering
{
    /**
     * @brief Stores a texture binding and submission where it becomes available.
     */
    struct STextureBinding
    {
        /** @brief Handle of the texture in the texture set. */
        SResourceHandle Handle{};

        /** @brief First submission that can use Handle. */
        uint64_t ReadySubmission = 0;

        /**
         * @brief Gets the texture index that is safe for a submission.
         * @param submissionSerial Serial of the submission being prepared.
         * @param fallbackIndex Index to use before the binding is available.
         * @return The texture index, or fallbackIndex when the binding is not ready.
         */
        uint32_t GetIndexForSubmission(
            const uint64_t submissionSerial,
            const uint32_t fallbackIndex
        ) const
        {
            return ReadySubmission <= submissionSerial
                ? Handle.Index
                : fallbackIndex;
        }
    };

    /**
     * @brief Manages bindless texture bindings used by material rendering.
     *
     * Newly added bindings use the fallback texture until descriptor updates become
     * visible to a later submission.
     */
    class ELIXIR_API TextureRegistry final
    {
    public:
        /**
         * @brief Creates a registry and its fallback texture resources.
         * @param context Graphics context that owns the texture resources.
         * @pre context is valid.
         */
        explicit TextureRegistry(const GraphicsContext* context);

        /**
         * @brief Starts texture resolution for a frame submission.
         * @param submissionSerial Serial of the submission being prepared.
         */
        void BeginFrame(uint64_t submissionSerial);

        /**
         * @brief Resolves a texture to an index in the material texture set.
         *
         * A newly registered texture returns the fallback index until it is ready.
         *
         * @param texture Texture to resolve.
         * @return A usable texture index.
         */
        uint32_t Resolve(const Ref<Texture>& texture);

        /**
         * @brief Finds the usable index for an already resolved texture.
         * @param texture Texture to find.
         * @return The texture index, or the fallback index when unavailable.
         */
        uint32_t Find(const Ref<Texture>& texture) const;

        /** @brief Gets the index of the fallback texture. */
        uint32_t GetFallbackIndex() const { return m_FallbackTextureHandle.Index; }

        /** @brief Gets the texture set used by material rendering. */
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures; }

        /** @brief Gets the sampler used with material textures. */
        const Ref<Sampler>& GetSampler() const { return m_Sampler; }

    private:
        Ref<TextureSet> m_Textures;
        Ref<Sampler> m_Sampler;
        SResourceHandle m_FallbackTextureHandle;
        std::unordered_map<Ref<Texture>, STextureBinding> m_Bindings;

        uint64_t m_SubmissionSerial = 0;

        const GraphicsContext* m_GraphicsContext;
    };
}