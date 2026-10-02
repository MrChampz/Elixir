#include "epch.h"
#include "TextureRegistry.h"

#include <Engine/Core/Color.h>
#include <Engine/Graphics/SamplerBuilder.h>

namespace Elixir::Materials::Rendering
{
    TextureRegistry::TextureRegistry(const GraphicsContext* context)
      : m_Textures(TextureSet::Create(context)),
        m_Sampler(SamplerBuilder().Build(context)),
        m_BindingState(CreateRef<SBindingState>()),
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
        m_ReferencedTextures.clear();
    }

    void TextureRegistry::EndFrame()
    {
        auto& bindings = m_BindingState->Bindings;
        for (auto binding = bindings.begin(); binding != bindings.end();)
        {
            if (m_ReferencedTextures.contains(binding->first))
            {
                ++binding;
                continue;
            }

            const Ref<Texture> texture = binding->first;
            const SResourceHandle handle = binding->second.Handle;
            binding = bindings.erase(binding);

            const Ref<SBindingState> state = m_BindingState;
            const Ref<TextureSet> textures = m_Textures;
            const uint64_t retirement = state->NextRetirement++;
            state->Retirements[texture] = retirement;
            m_GraphicsContext->DeferResourceRelease([state, textures, texture, handle, retirement]()
            {
                const auto retired = state->Retirements.find(texture);
                if (retired == state->Retirements.end() || retired->second != retirement)
                    return;

                state->Retirements.erase(retired);
                textures->RemoveTexture(handle);
            });
        }
    }

    uint32_t TextureRegistry::Resolve(const Ref<Texture>& texture)
    {
        if (!texture)
            return GetFallbackIndex();

        m_ReferencedTextures.insert(texture);
        m_BindingState->Retirements.erase(texture);

        const auto found = m_BindingState->Bindings.find(texture);
        if (found != m_BindingState->Bindings.end())
            return Find(texture);

        // Bindless descriptor updates become visible before the next render callback.
        const auto handle = m_Textures->AddTexture(texture);
        m_BindingState->Bindings.emplace(texture, STextureBinding{
            .Handle = handle,
            .ReadySubmission = m_SubmissionSerial + 1,
        });

        return GetFallbackIndex();
    }

    uint32_t TextureRegistry::Find(const Ref<Texture>& texture) const
    {
        if (!texture)
            return GetFallbackIndex();

        const auto found = m_BindingState->Bindings.find(texture);
        if (found == m_BindingState->Bindings.end())
            return GetFallbackIndex();

        return found->second.GetIndexForSubmission(
            m_SubmissionSerial,
            GetFallbackIndex()
        );
    }
}
