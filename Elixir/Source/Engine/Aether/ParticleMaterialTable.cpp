#include "epch.h"
#include "ParticleMaterialTable.h"

namespace Elixir::Aether
{
    std::optional<uint32_t> ParticleMaterialTable::Add(const MaterialRenderProxy& material)
    {
        const auto found = m_Indices.find(&material);
        if (found != m_Indices.end())
            return found->second;

        if (m_Data.size() >= m_Capacity)
            return std::nullopt;

        const auto index = (uint32_t)m_Data.size();
        m_Data.push_back(BuildData(material));
        m_Indices.emplace(&material, index);
        return index;
    }

    SParticleMaterialData ParticleMaterialTable::BuildData(
        const MaterialRenderProxy& material
    ) const
    {
        SParticleMaterialData data{};
        std::ranges::fill(data.TextureIndices, m_FallbackTextureIndex);

        const auto& values = material.GetValues();
        std::copy_n(
            values.begin(),
            std::min(values.size(), data.Values.size()),
            data.Values.begin()
        );

        const auto& textures = material.GetTextures();
        const auto textureCount = std::min(textures.size(), data.TextureIndices.size());

        for (size_t slot = 0; slot < textureCount; ++slot)
        {
            if (textures[slot])
                data.TextureIndices[slot] = m_TextureIndexResolver(textures[slot]);
        }

        return data;
    }
}
