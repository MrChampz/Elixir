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

        /**
         * @brief Import static geometry and materials.
         * @param context Graphics context used to create material textures.
         * @param path Source file to import.
         * @return Mesh data, or no value when import fails.
         */
        virtual std::optional<SStaticMeshData> LoadStaticMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path
        ) const = 0;

        /**
         * @brief Import skeletal geometry, materials, and animations.
         * @param context Graphics context used to create material textures.
         * @param path Source file to import.
         * @param skeleton Optional shared skeleton compatible with the source hierarchy.
         * @return Mesh data, or no value when import fails or no skinned mesh is present.
         */
        virtual std::optional<SSkeletalMeshData> LoadSkeletalMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path,
            Ref<const Skeleton> skeleton = nullptr
        ) const = 0;
    };
}
