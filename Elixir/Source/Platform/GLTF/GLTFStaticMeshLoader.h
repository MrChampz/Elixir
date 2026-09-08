#pragma once

#include "fastgltf/types.hpp"

#include <Engine/Mesh/StaticMeshLoader.h>

namespace fastgltf { class Mesh; }

namespace Elixir
{
    /** @brief Imports static meshes from glTF and GLB source files. */
    class GLTFStaticMeshLoader final : public StaticMeshLoader
    {
      public:
        /**
         * @brief Get the glTF source format supported by this loader */
        EStaticMeshFormat GetFormat() const override { return EStaticMeshFormat::GLTF; }

        /**
         * @brief Import all static meshes stored in one glTF or GLB file.
         * @param request Source file and graphics context.
         * @return Imported meshes and diagnostics.
         */
        SStaticMeshLoadResult Load(const SStaticMeshLoadRequest& request) const override;

    private:
        static void LoadMesh(
            const SStaticMeshLoadRequest& request,
            SStaticMeshLoadResult& result,
            const fastgltf::Asset& asset,
            const fastgltf::Mesh& mesh
        );

        static void LoadPrimitive(
            const SStaticMeshLoadRequest& request,
            SStaticMeshLoadResult& result,
            SStaticMeshCreateInfo& mesh,
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& primitive,
            bool& hasBounds
        );
    };
}