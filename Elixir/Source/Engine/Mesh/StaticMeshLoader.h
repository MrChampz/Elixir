#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    class GraphicsContext;

    /** @brief Supplies the engine state and source file for one mesh import. */
    struct SStaticMeshLoadRequest
    {
        const GraphicsContext* GraphicsContext = nullptr;
        std::filesystem::path Path;
    };

    /** @brief Contains the meshes loaded from one asset path, when loading succeeds. */
    using StaticMeshLoadResult = std::optional<std::vector<Ref<StaticMesh>>>;

    /**
     * @brief Imports one static mesh source format into engine assets.
     *
     * Implementations must not expose parser-specific types through this contract.
     */
    class ELIXIR_API StaticMeshLoader
    {
    public:
        virtual ~StaticMeshLoader() = default;

        /**
         * @brief Import all valid static meshes from one source file.
         * @param request Source file and graphics context.
         * @return Loaded meshes, or std::nullopt when loading fails.
         */
        virtual StaticMeshLoadResult Load(const SStaticMeshLoadRequest& request) const = 0;
    };
}
