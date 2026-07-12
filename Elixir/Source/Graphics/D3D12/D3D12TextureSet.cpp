#include "epch.h"
#include "D3D12TextureSet.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12Image.h>

namespace Elixir::D3D12
{
    D3D12TextureSet::D3D12TextureSet(const GraphicsContext* context)
        : TextureSet(context)
    {
    }

    void D3D12TextureSet::Clear()
    {
        m_Textures.clear();
        m_TexturesByIndex.clear();
        m_TextureCount = 0;
        m_NextTextureIndex = 0;
    }

    SResourceHandle D3D12TextureSet::AddTexture(const Ref<Texture>& texture)
    {
        if (m_Textures.contains(texture))
            return m_Textures.at(texture);

        const auto index = m_NextTextureIndex++;
        const auto handle = SResourceHandle::Texture(index);
        m_Textures[texture] = handle;
        m_TexturesByIndex[index] = texture;
        m_TextureCount = (uint32_t)m_Textures.size();
        return handle;
    }

    void D3D12TextureSet::RemoveTexture(const SResourceHandle handle)
    {
        if (!handle.IsValid())
            return;

        if (const auto it = m_TexturesByIndex.find(handle.Index); it != m_TexturesByIndex.end())
        {
            m_Textures.erase(it->second);
            m_TexturesByIndex.erase(it);
            m_TextureCount = (uint32_t)m_Textures.size();
        }
    }
}

#endif
