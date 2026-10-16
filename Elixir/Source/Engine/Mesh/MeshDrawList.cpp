#include "epch.h"
#include "MeshDrawList.h"

#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Materials/Material.h>

namespace Elixir
{
    SMeshDrawList BuildMeshDrawList(
        const std::span<const SStaticMeshVertex> vertices,
        const std::span<const uint32_t> indices,
        const std::span<const SMeshSection> sections,
        const std::span<const EMaterialBlendMode> blendModes,
        const glm::mat4& view
    )
    {
        struct STriangle
        {
            std::array<uint32_t, 3> Indices;
            uint32_t MaterialIndex;
            float Depth;
        };

        SMeshDrawList result;
        result.Indices.reserve(indices.size());
        std::vector<STriangle> translucent;

        for (const auto& section : sections)
        {
            if (blendModes[section.MaterialIndex] == EMaterialBlendMode::Translucent)
            {
                const auto indexCount = size_t(section.FirstIndex) + section.IndexCount;
                for (size_t i = section.FirstIndex; i < indexCount; i += 3)
                {
                    const std::array triangle{
                        indices[i]     + section.VertexOffset,
                        indices[i + 1] + section.VertexOffset,
                        indices[i + 2] + section.VertexOffset,
                    };

                    const auto position = vertices[triangle[0]].Position +
                        vertices[triangle[1]].Position +
                        vertices[triangle[2]].Position;
                    const auto center = position / 3.0f;

                    translucent.push_back({
                        triangle,
                        section.MaterialIndex,
                        -(view * glm::vec4(center, 1)).z
                    });
                }

                continue;
            }

            if (section.IndexCount == 0) continue;

            const auto firstIndex = (uint32_t)result.Indices.size();
            const auto indexCount = size_t(section.FirstIndex) + section.IndexCount;
            for (size_t i = section.FirstIndex; i < indexCount; ++i)
                result.Indices.push_back(indices[i] + section.VertexOffset);

            result.Ranges.push_back({
                firstIndex,
                section.IndexCount,
                section.MaterialIndex
            });
        }

        std::ranges::stable_sort(
            translucent,
            [](const STriangle& left, const STriangle& right)
            {
                return left.Depth > right.Depth;
            }
        );

        const auto opaqueRangeCount = result.Ranges.size();
        for (const auto& triangle : translucent)
        {
            if (result.Ranges.size() == opaqueRangeCount ||
                result.Ranges.back().MaterialIndex != triangle.MaterialIndex)
            {
                result.Ranges.push_back({
                    (uint32_t)result.Indices.size(),
                    0,
                    triangle.MaterialIndex,
                    triangle.Depth
                });
            }

            result.Indices.insert(
                result.Indices.end(),
                triangle.Indices.begin(),
                triangle.Indices.end()
            );

            result.Ranges.back().IndexCount += 3;
        }

        return result;
    }
}
