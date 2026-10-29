#include "epch.h"
#include "TextureRegistry.h"

#include <Engine/Core/Color.h>
#include <Engine/Graphics/SamplerBuilder.h>

namespace Elixir::Materials::Rendering
{
    TextureRegistry::TextureRegistry(const GraphicsContext* context)
      : m_Textures(ImageSet::Create(context)),
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
        m_ReferencedImages.clear();
    }

    void TextureRegistry::EndFrame()
    {
        auto& bindings = m_BindingState->Bindings;
        for (auto binding = bindings.begin(); binding != bindings.end();)
        {
            if (m_ReferencedImages.contains(binding->first))
            {
                ++binding;
                continue;
            }

            const Ref<Image> image = binding->first;
            const SResourceHandle handle = binding->second.Handle;
            binding = bindings.erase(binding);

            const Ref<SBindingState> state = m_BindingState;
            const Ref<ImageSet> textures = m_Textures;

            const uint64_t retirement = state->NextRetirement++;
            state->Retirements[image] = retirement;
            
            m_GraphicsContext->DeferResourceRelease(
                [state, textures, image, handle, retirement]()
                {
                    const auto retired = state->Retirements.find(image);
                    if (retired == state->Retirements.end() || retired->second != retirement)
                        return;

                    state->Retirements.erase(retired);
                    textures->RemoveImage(handle);
                }
            );
        }
    }

    std::optional<uint32_t> TextureRegistry::Resolve(const Ref<Texture>& texture)
    {
        if (!m_FallbackTextureHandle.IsValid())
            return std::nullopt;

        if (!texture)
            return GetFallbackIndex();

        const auto& image = texture->GetImage();
        if (!image) return std::nullopt;

        m_ReferencedImages.insert(image);
        m_BindingState->Retirements.erase(image);

        const auto found = m_BindingState->Bindings.find(texture->GetImage());
        if (found != m_BindingState->Bindings.end())
            return Find(texture);

        // Bindless descriptor updates become visible before the next render callback.
        const auto handle = m_Textures->AddImage(image);
        if (!handle.IsValid())
            return std::nullopt;

        // The fallback registration remains owned by the registry for its entire lifetime.
        if (handle == m_FallbackTextureHandle)
            return GetFallbackIndex();

        m_BindingState->Bindings.emplace(image, STextureBinding{
            .Handle = handle,
            .ReadySubmission = m_SubmissionSerial + 1,
        });

        return GetFallbackIndex();
    }

    uint32_t TextureRegistry::Find(const Ref<Texture>& texture) const
    {
        if (!texture)
            return GetFallbackIndex();

        const auto found = m_BindingState->Bindings.find(texture->GetImage());
        if (found == m_BindingState->Bindings.end())
            return GetFallbackIndex();

        return found->second.GetIndexForSubmission(
            m_SubmissionSerial,
            GetFallbackIndex()
        );
    }
}
