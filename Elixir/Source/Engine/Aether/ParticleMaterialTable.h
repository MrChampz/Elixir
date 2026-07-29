#pragma once

#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir::Aether
{
    // ABI shared with Shaders/Material/ParticleSprite.ps.hlsl.
    struct alignas(16) SParticleMaterialData
    {
        std::array<glm::vec4, 32> Values{};
        std::array<uint32_t, 32> TextureIndices{};
    };

    // Renderer-owned, frame-local staging data. It does not contain mutable
    // particle simulation state.
    class ELIXIR_API ParticleMaterialTable final
    {
    public:
        explicit ParticleMaterialTable(const uint32_t capacity) : m_Capacity(capacity) {}

        // Returns the stable index for this render submission, or nullopt when
        // the per-frame capacity is exhausted.
        std::optional<uint32_t> Add(const MaterialRenderProxy& material);

        const std::vector<SParticleMaterialData>& GetData() const { return m_Data; }
        uint32_t GetCount() const { return static_cast<uint32_t>(m_Data.size()); }

    private:
        static SParticleMaterialData BuildData(const MaterialRenderProxy& material);

        uint32_t m_Capacity = 0;
        std::unordered_map<const MaterialRenderProxy*, uint32_t> m_Indices;
        std::vector<SParticleMaterialData> m_Data;
    };
}