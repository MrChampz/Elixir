#include "epch.h"
#include "FrameTable.h"

namespace Elixir::Materials::Rendering
{
    FrameTable::FrameTable(
        const uint32_t capacity,
        const uint32_t fallbackTextureIndex,
        TextureIndexResolver resolver
    ) : m_Capacity(capacity),
        m_FallbackTextureIndex(fallbackTextureIndex),
        m_TextureIndexResolver(std::move(resolver)) {}

    std::optional<uint32_t> FrameTable::Add(const MaterialRenderProxy& material)
    {
        if (const auto found = Find(material))
            return found;

        if (m_Data.size() >= m_Capacity)
            return std::nullopt;

        const auto index = (uint32_t)m_Data.size();
        m_Data.push_back(BuildData(material));
        m_Indices.emplace(&material, index);
        return index;
    }

    std::optional<uint32_t> FrameTable::Find(const MaterialRenderProxy& material) const
    {
        const auto found = m_Indices.find(&material);
        if (found == m_Indices.end())
            return std::nullopt;

        return found->second;
    }

    SMaterialFrameData FrameTable::BuildData(const MaterialRenderProxy& material) const
    {
        SMaterialFrameData data{};
        std::ranges::fill(data.TextureIndices, m_FallbackTextureIndex);

        const auto& values = material.GetValues();
        const auto valueCount = std::min(values.size(), data.Values.size());
        std::copy_n(values.begin(), valueCount, data.Values.begin());

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
