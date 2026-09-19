#pragma once

#include <Engine/Mesh/StaticMeshLoader.h>

namespace Elixir
{
    /** @brief Imports static meshes from glTF and GLB source files. */
    class GLTFStaticMeshLoader final : public StaticMeshLoader
    {
      public:
        /**
         * @brief Combine all glTF meshes into one static mesh data object.
         * @param context Graphics context available for material resolution.
         * @param path Local glTF or GLB file path.
         * @return Loaded mesh data, or std::nullopt when loading fails.
         */
        std::optional<SStaticMeshData> Load(
            const GraphicsContext& context,
            std::filesystem::path path
        ) const override;
    };
}
