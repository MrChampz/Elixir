#pragma once

#include <Engine/Graphics/StaticMeshLoader.h>

namespace Elixir
{
    /** @brief Imports static meshes from glTF and GLB source files. */
    class ELIXIR_API GltfStaticMeshLoader final : public StaticMeshLoader
    {
      public:
        /** @brief Get the glTF source format supported by this loader. */
        [[nodiscard]] EStaticMeshFormat GetFormat() const override
        {
            return EStaticMeshFormat::Gltf;
        }
        /**
         * @brief Import all static meshes stored in one glTF or GLB file.
         * @param request Source file and graphics context.
         * @return Imported meshes and diagnostics.
         */
        [[nodiscard]] SStaticMeshLoadResult Load(
            const SStaticMeshLoadRequest& request
        ) const override;
    };
}
