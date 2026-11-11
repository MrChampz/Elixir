#include "epch.h"
#include "TextureAtlas.h"

namespace Elixir
{
    Ref<TextureAtlas> TextureAtlas::Create(
        const Ref<Texture>& texture,
        const STextureAtlasLayout layout
    )
    {
        if (!texture ||
            !texture->IsValid() ||
            texture->GetType() != EImageType::_2D ||
            !layout.IsValid() ||
            texture->GetWidth() < layout.Columns ||
            texture->GetHeight() < layout.Rows)
        {
            EE_CORE_ERROR("Texture atlas requires a valid 2D texture and cells at least one texel.")
            return nullptr;
        }

        return Ref<TextureAtlas>(new TextureAtlas(texture, layout));
    }

    TextureAtlas::TextureAtlas(Ref<Texture> texture, const STextureAtlasLayout layout)
      : m_Texture(std::move(texture)),
        m_Layout(layout) {}
}
