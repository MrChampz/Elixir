#include "epch.h"
#include "StaticMesh.h"

#include <Engine/Mesh/GeometryPool.h>

namespace Elixir
{
    Ref<StaticMesh> StaticMesh::Create(
        std::string name,
        GeometryAllocation geometry,
        std::vector<SStaticMeshSection> sections,
        std::vector<Ref<Material>> materials,
        SStaticMeshBounds localBounds
    )
    {
        if (!geometry.IsValid())
        {
            EE_CORE_ERROR("StaticMesh requires a valid geometry handle.")
            return nullptr;
        }

        if (sections.empty())
        {
            EE_CORE_ERROR("StaticMesh requires at least one section.")
            return nullptr;
        }

        if (materials.empty())
        {
            EE_CORE_ERROR("StaticMesh requires at least one material.")
            return nullptr;
        }

        for (const auto& material : materials)
        {
            if (!material)
            {
                EE_CORE_ERROR("StaticMesh cannot contain a null material.")
                return nullptr;
            }
        }

        for (const auto& section : sections)
        {
            if (section.MaterialIndex >= materials.size())
            {
                EE_CORE_ERROR("StaticMesh section references an invalid material index.")
                return nullptr;
            }
        }

        return Ref<StaticMesh>(new StaticMesh(
            std::move(name),
            std::move(geometry),
            std::move(sections),
            std::move(materials),
            localBounds
        ));
    }

    const BufferLayout& StaticMesh::GetVertexLayout()
    {
        static const BufferLayout layout = {{
            { EDataType::Vec3, "Position" },
            { EDataType::Vec3, "Normal" },
            { EDataType::Vec4, "Tangent" },
            { EDataType::Vec2, "TexCoord" },
        }};
        return layout;
    }

    StaticMesh::StaticMesh(
        std::string name,
        GeometryAllocation geometry,
        std::vector<SStaticMeshSection> sections,
        std::vector<Ref<Material>> materials,
        const SStaticMeshBounds& localBounds
    ) : m_Name(std::move(name)),
        m_Geometry(std::move(geometry)),
        m_Sections(std::move(sections)),
        m_Materials(std::move(materials)),
        m_LocalBounds(localBounds) {}
}
