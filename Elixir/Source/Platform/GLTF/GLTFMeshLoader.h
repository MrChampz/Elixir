#pragma once

#include <Engine/Mesh/MeshLoader.h>

namespace Elixir
{
    /** @brief Imports static and skeletal models through one glTF/GLB source implementation. */
    class ELIXIR_API GLTFMeshLoader final : public MeshLoader
    {
    public:
        /** @brief Bake selected scene geometry into one static mesh with Surface materials. */
        std::optional<SStaticMeshData> LoadStaticMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path
        ) const override;

        /** @brief Import all model sections and skin bindings with their animation assets. */
        std::optional<SSkeletalMeshData> LoadSkeletalMesh(
            const GraphicsContext& context,
            const std::filesystem::path& path,
            Ref<const Skeleton> skeleton = nullptr
        ) const override;
    };
}
