#pragma once

#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/TextureSet.h>

namespace Elixir
{
    struct SMaterialTextureBinding
    {
        SResourceHandle Handle{};
        uint64_t ReadySubmission = 0;

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

    class ELIXIR_API MaterialTextureRegistry final
    {
    public:
        explicit MaterialTextureRegistry(const GraphicsContext* context);

        void BeginFrame(uint64_t submissionSerial);
        uint32_t Resolve(const Ref<Texture>& texture);
        uint32_t Find(const Ref<Texture>& texture) const;

        uint32_t GetFallbackIndex() const { return m_FallbackTextureHandle.Index; }
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures; }
        const Ref<Sampler>& GetSampler() const { return m_Sampler; }

    private:
        Ref<TextureSet> m_Textures;
        Ref<Sampler> m_Sampler;
        SResourceHandle m_FallbackTextureHandle;
        std::unordered_map<Ref<Texture>, SMaterialTextureBinding> m_Bindings;

        uint64_t m_SubmissionSerial = 0;

        const GraphicsContext* m_GraphicsContext;
    };
}