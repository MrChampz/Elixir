#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    class BufferLayout;
    namespace Materials { class Material; }

    /** @brief Defines an axis-aligned bound in mesh-local space. */
    struct SMeshBounds
    {
        glm::vec3 Min{};
        glm::vec3 Max{};
    };

    /** @brief Describes one drawable range in a mesh. */
    struct SMeshSection
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        uint32_t VertexOffset = 0;
        uint32_t MaterialIndex = 0;
        SMeshBounds LocalBounds;

        /** Maps baked mesh directions back to source mesh space for volume thickness. */
        glm::mat3 VolumeToLocal{1.0f};
    };

    /** @brief Get the Surface vertex layout shared by both mesh renderers. */
    ELIXIR_API const BufferLayout& GetMeshVertexLayout();
}
