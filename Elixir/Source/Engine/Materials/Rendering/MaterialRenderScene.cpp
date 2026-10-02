#include "epch.h"
#include "MaterialRenderScene.h"

namespace Elixir::Materials::Rendering
{
    /* SMaterialPushConstants */

    std::array<std::byte, SMaterialPushConstants::CAPACITY>
    SMaterialPushConstants::Resolve(const uint32_t materialIndex) const
    {
        auto resolved = Data;

        const auto patch = [&resolved](const uint32_t offset, const uint32_t value)
        {
            if (offset == NO_OFFSET) return;

            EE_CORE_ASSERT(
                offset + sizeof(value) <= CAPACITY,
                "Material push constant index offset is out of range."
            )

            Memory::Memcpy(resolved.data() + offset, &value, sizeof(value));
        };

        patch(MaterialIndexOffset, materialIndex);

        return resolved;
    }

    /* MaterialRenderScene */

    uint32_t MaterialRenderScene::AddGeometry(SRenderGeometry geometry)
    {
        EE_CORE_ASSERT(
            geometry.Pipeline.VertexLayout,
            "Material render geometry requires a vertex layout."
        )

        const auto index = (uint32_t)m_Geometries.size();
        m_Geometries.push_back(std::move(geometry));

        return index;
    }

    void MaterialRenderScene::Add(SRenderItem item)
    {
        EE_CORE_ASSERT(
            item.GeometryIndex < m_Geometries.size(),
            "Material render item references a unknown geometry."
        )

        m_Items.push_back(std::move(item));
    }

    const SRenderGeometry* MaterialRenderScene::FindGeometry(const uint32_t index) const
    {
        if (index >= m_Geometries.size())
            return nullptr;

        return &m_Geometries[index];
    }
}
