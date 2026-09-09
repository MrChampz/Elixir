#include "epch.h"
#include "StaticMesh.h"

namespace Elixir
{
    Ref<StaticMesh> StaticMesh::Create(SStaticMeshCreateInfo info)
    {
        const bool hasValidSection = std::ranges::any_of(info.Sections, [](const auto& section)
        {
            return section.Vertices && section.Indices && section.IndexCount > 0;
        });

        if (!hasValidSection)
        {
            EE_CORE_ERROR("StaticMesh requires at least one valid section.")
            return nullptr;
        }

        return Ref<StaticMesh>(new StaticMesh(std::move(info)));
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

    StaticMesh::StaticMesh(SStaticMeshCreateInfo info)
      : m_Name(std::move(info.Name)),
        m_Sections(std::move(info.Sections)),
        m_LocalBounds(info.LocalBounds) {}
}
