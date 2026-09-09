#pragma once

#include <Engine/Mesh/StaticMeshLoader.h>

namespace Elixir
{
    struct SGLTFStaticMeshImportOptions
    {
        /** Merge all valid glTF meshes into one static mesh. */
        bool MergeMeshes = true;
    };

    /** @brief Imports static meshes from glTF and GLB source files. */
    class GLTFStaticMeshLoader final : public StaticMeshLoader
    {
      public:
        explicit GLTFStaticMeshLoader(SGLTFStaticMeshImportOptions = {});

        /**
         * @brief Import all static meshes stored in one glTF or GLB file.
         * @param request Source file and graphics context.
         * @return Loaded meshes, or std::nullopt when loading fails.
         */
        StaticMeshLoadResult Load(const SStaticMeshLoadRequest& request) const override;

    private:
        SGLTFStaticMeshImportOptions m_Options;
    };
}
