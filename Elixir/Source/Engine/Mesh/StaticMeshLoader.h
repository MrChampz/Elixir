#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    class GraphicsContext;

    /**
     * @brief Loads runtime data for one static mesh from an asset source.
     *
     * Implementations must not expose parser-specific types through this contract.
     */
    class ELIXIR_API StaticMeshLoader
    {
    public:
        virtual ~StaticMeshLoader() = default;

        /**
         * @brief Load static mesh data from one source file.
         *
         * The graphics context enable loaders to resolve material textures and
         * other graphics resources required by the loaded materials.
         *
         * @param context Graphics context used by the loader.
         * @param path Local source file path.
         * @return Loaded mesh data, or std::nullopt when loading fails.
         */
        virtual std::optional<SStaticMeshData> Load(
            const GraphicsContext& context,
            std::filesystem::path path
        ) const = 0;
    };
}
