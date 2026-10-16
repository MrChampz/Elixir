#pragma once

#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir
{
    class GraphicsContext;

    /** @brief Imports typed mesh data without exposing source parser types. */
    class ELIXIR_API MeshLoader
    {
    public:
        /** @brief Release the source loader. */
        virtual ~MeshLoader() = default;

        /** @brief Import static geometry and materials, or return no result on failure. */
        virtual std::optional<SStaticMeshData> LoadStaticMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path
        ) const = 0;

        /**
         * @brief Import a complete skeletal model with its materials and animations.
         * @param skeleton Optional shared skeleton compatible with the source hierarchy.
         */
        virtual std::optional<SSkeletalMeshData> LoadSkeletalMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path,
            Ref<const Skeleton> skeleton = nullptr
        ) const = 0;
    };
}
