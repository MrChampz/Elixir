#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    /**
     * @brief Loads CPU data for one static mesh from an asset source.
     *
     * Implementations must not expose parser-specific types through this contract.
     */
    class ELIXIR_API StaticMeshLoader
    {
    public:
        virtual ~StaticMeshLoader() = default;

        /**
         * @brief Load CPU mesh data from one source file.
         * @param path Local source file path.
         * @return Loaded mesh data, or std::nullopt when loading fails.
         */
        virtual std::optional<SStaticMeshData> Load(std::filesystem::path path) const = 0;
    };
}
