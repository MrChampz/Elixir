#pragma once

#include <Engine/Mesh/Mesh.h>
#include <span>

namespace Elixir
{
    struct SStaticMeshVertex;
    namespace Materials { enum class EMaterialBlendMode : uint8_t; }
    using namespace Materials;

    /** @brief Describes a draw using absolute vertex indices and its camera depth. */
    struct SMeshDrawRange
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        uint32_t MaterialIndex = 0;
        /** Positive camera depth used for back-to-front transparency sorting. */
        float SortDepth = 0.0f;
    };

    /** @brief Stores frame-local indices and draws for a posed mesh. */
    struct SMeshDrawList
    {
        std::vector<uint32_t> Indices;
        std::vector<SMeshDrawRange> Ranges;
    };

    /**
     * @brief Build mesh draws, sorting translucent triangles from far to near after deformation.
     * Consecutive translucent triangles sharing a material use one draw.
     * @pre Sections contain valid triangle ranges and material indices; vertices are in model space.
     */
    ELIXIR_API SMeshDrawList BuildMeshDrawList(
        std::span<const SStaticMeshVertex> vertices,
        std::span<const uint32_t> indices,
        std::span<const SMeshSection> sections,
        std::span<const EMaterialBlendMode> blendModes,
        const glm::mat4& view
    );
}
