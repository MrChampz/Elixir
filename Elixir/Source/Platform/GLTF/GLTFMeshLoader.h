#pragma once

#include <Engine/Mesh/MeshLoader.h>

namespace Elixir
{
    /**
     * @brief Imports static and skeletal models through one glTF/GLB source implementation.
     */
    class ELIXIR_API GLTFMeshLoader final : public MeshLoader
    {
    public:
        /**
         * @brief Bake selected scene geometry into a static mesh with Surface materials.
         * @param context Graphics context used to create material textures.
         * @param path Source file to import.
         * @return Mesh data, or no value for an invalid or unsupported source.
         */
        std::optional<SStaticMeshData> LoadStaticMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path
        ) const override;

        /**
         * @brief Import model sections, skin bindings, and animation assets.
         * @param context Graphics context used to create material textures.
         * @param path Source file to import.
         * @param skeleton Optional shared skeleton compatible with the source hierarchy.
         * @return Mesh data, or no value for an invalid source or a source without skinned
         *     geometry.
         */
        std::optional<SSkeletalMeshData> LoadSkeletalMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path,
            Ref<const Skeleton> skeleton = nullptr
        ) const override;
    };
}
