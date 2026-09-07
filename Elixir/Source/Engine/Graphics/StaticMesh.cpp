#include "epch.h"
#include "StaticMesh.h"

namespace Elixir
{
    Ref<StaticMesh> StaticMesh::Create(SStaticMeshCreateInfo createInfo)
    {
        const bool hasValidSection = std::ranges::any_of(createInfo.Sections, [](const auto& section)
        {
            return section.Vertices && section.Indices && section.IndexCount > 0;
        });
        if (!hasValidSection)
        {
            EE_CORE_ERROR("StaticMesh requires at least one valid section.")
            return nullptr;
        }

        return Ref<StaticMesh>(new StaticMesh(std::move(createInfo)));
    }

    StaticMesh::StaticMesh(SStaticMeshCreateInfo createInfo)
      : m_Name(std::move(createInfo.Name)),
        m_Sections(std::move(createInfo.Sections)),
        m_LocalBounds(createInfo.LocalBounds)
    {
    }
}
