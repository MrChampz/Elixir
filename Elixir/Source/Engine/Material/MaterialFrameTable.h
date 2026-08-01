#pragma once

#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir
{
    // GPU ABI shared by material templates.
    struct alignas(16) SMaterialFrameData
    {
        std::array<glm::vec4, 32>   Values{};
        std::array<uint32_t, 32>    TextureIndices{};
    };

    class ELIXIR_API MaterialFrameTable final
    {
    public:
        using TextureIndexResolver = std::function<uint32_t(const Ref<Texture>&)>;

        MaterialFrameTable(
            uint32_t capacity,
            uint32_t fallbackTextureIndex,
            TextureIndexResolver resolver
        );

        std::optional<uint32_t> Add(const MaterialRenderProxy& material);
        std::optional<uint32_t> Find(const MaterialRenderProxy& material) const;

        const std::vector<SMaterialFrameData>& GetData() const { return m_Data; }
        uint32_t GetCount() const { return static_cast<uint32_t>(m_Data.size()); }

    private:
        SMaterialFrameData BuildData(const MaterialRenderProxy& material) const;

        uint32_t m_Capacity = 0;
        uint32_t m_FallbackTextureIndex = 0;
        TextureIndexResolver m_TextureIndexResolver;
        std::unordered_map<const MaterialRenderProxy*, uint32_t> m_Indices;
        std::vector<SMaterialFrameData> m_Data;
    };
}