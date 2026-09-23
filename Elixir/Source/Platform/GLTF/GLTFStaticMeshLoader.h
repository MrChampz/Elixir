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
         *
         * The loader creates one Surface material for every glTF material and
         * maps glTF alpha modes to the material blend mode. It
         * imports metallic-roughness PBR factors and base-color,
         * metallic-roughness, normal, occlusion, and emissive maps. Materials
         * that use `KHR_materials_clearcoat` use the ClearCoat shading model
         * and import the clear-coat factor, roughness, and normal map.
         * `KHR_materials_specular` imports the dielectric specular factor,
         * tint, and optional textures.
         *
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
