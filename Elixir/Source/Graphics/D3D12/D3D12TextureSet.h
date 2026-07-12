#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/TextureSet.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    class ELIXIR_API D3D12TextureSet final : public TextureSet
    {
      public:
        explicit D3D12TextureSet(const GraphicsContext* context);
        ~D3D12TextureSet() override = default;

        void Clear() override;
        SResourceHandle AddTexture(const Ref<Texture>& texture) override;
        void RemoveTexture(SResourceHandle handle) override;

        const auto& GetTexturesByIndex() const { return m_TexturesByIndex; }

      private:
        std::unordered_map<uint32_t, Ref<Texture>> m_TexturesByIndex;
        uint32_t m_NextTextureIndex = 0;
    };
}

#endif
