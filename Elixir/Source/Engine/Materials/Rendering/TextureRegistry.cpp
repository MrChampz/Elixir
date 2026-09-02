#include "epch.h"
#include "TextureRegistry.h"

#include <Engine/Core/Color.h>
#include <Engine/Graphics/SamplerBuilder.h>

namespace Elixir::Materials::Rendering
{
    TextureRegistry::TextureRegistry(const GraphicsContext* context)
      : m_Textures(TextureSet::Create(context)),
        m_Sampler(SamplerBuilder().Build(context)),
        m_GraphicsContext(context)
    {
        const auto whiteTexture = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_SRGB,
            1,
            1,
            &Color::WhiteAlpha
        );

        m_FallbackTextureHandle = m_Textures->AddTexture(whiteTexture);
    }

    void TextureRegistry::BeginFrame(const uint64_t submissionSerial)
    {
        m_SubmissionSerial = submissionSerial;
    }

    uint32_t TextureRegistry::Resolve(const Ref<Texture>& texture)
    {
        if (!texture)
            return GetFallbackIndex();

        const auto found = m_Bindings.find(texture);
        if (found != m_Bindings.end())
            return Find(texture);

        // Bindless descriptor updates become visible before the next render callback.
        const auto handle = m_Textures->AddTexture(texture);
        m_Bindings.emplace(texture, STextureBinding{
            .Handle = handle,
            .ReadySubmission = m_SubmissionSerial + 1,
        });

        return GetFallbackIndex();
    }

    uint32_t TextureRegistry::Find(const Ref<Texture>& texture) const
    {
        if (!texture)
            return GetFallbackIndex();

        const auto found = m_Bindings.find(texture);
        if (found == m_Bindings.end())
            return GetFallbackIndex();

        return found->second.GetIndexForSubmission(
            m_SubmissionSerial,
            GetFallbackIndex()
        );
    }
}
