#include "epch.h"
#include "StaticMesh.h"

#include <Engine/Mesh/GeometryPool.h>

namespace Elixir
{
    Ref<StaticMesh> StaticMesh::Create(
        std::string name,
        GeometryAllocation geometry,
        std::vector<SStaticMeshSection> sections,
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

        return Ref<StaticMesh>(new StaticMesh(
            std::move(name),
            std::move(geometry),
            std::move(sections),
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
        const SStaticMeshBounds& localBounds
    ) : m_Name(std::move(name)),
        m_Geometry(std::move(geometry)),
        m_Sections(std::move(sections)),
        m_LocalBounds(localBounds) {}
}
