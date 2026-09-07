#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/StaticMesh.h>

namespace Elixir
{
    class GraphicsContext;

    /** @brief Identifies a source format supported by a static mesh loader. */
    enum class EStaticMeshFormat : uint8_t
    {
        Gltf,
    };

    /** @brief Supplies the engine state and source file for one mesh import. */
    struct SStaticMeshLoadRequest
    {
        /** @brief Graphics context that owns uploaded geometry buffers. */
        const GraphicsContext* GraphicsContext = nullptr;
        /** @brief Local glTF or GLB source file. */
        std::filesystem::path Path;
    };

    /** @brief Marks the severity of one static mesh import diagnostic. */
    enum class EStaticMeshLoadDiagnosticSeverity : uint8_t
    {
        Warning,
        Error,
    };

    /** @brief Reports an import condition without exposing a parser implementation. */
    struct SStaticMeshLoadDiagnostic
    {
        /** @brief Severity of this condition. */
        EStaticMeshLoadDiagnosticSeverity Severity = EStaticMeshLoadDiagnosticSeverity::Error;
        /** @brief Human-readable engine diagnostic. */
        std::string Message;
    };

    /** @brief Contains all static meshes produced from one source file. */
    struct SStaticMeshLoadResult
    {
        /** @brief Imported meshes, one for each valid source mesh. */
        std::vector<Ref<StaticMesh>> Meshes;
        /** @brief Warnings and errors produced while importing. */
        std::vector<SStaticMeshLoadDiagnostic> Diagnostics;

        /** @brief Check whether the import produced an error. */
        [[nodiscard]] bool HasErrors() const;
    };

    /**
     * @brief Imports one static mesh source format into engine assets.
     *
     * Implementations must not expose parser-specific types through this contract.
     */
    class ELIXIR_API StaticMeshLoader
    {
      public:
        virtual ~StaticMeshLoader() = default;

        /** @brief Get the source format accepted by this loader. */
        [[nodiscard]] virtual EStaticMeshFormat GetFormat() const = 0;
        /**
         * @brief Import all valid static meshes from one source file.
         * @param request Source file and graphics context.
         * @return Imported meshes and diagnostics.
         */
        [[nodiscard]] virtual SStaticMeshLoadResult Load(
            const SStaticMeshLoadRequest& request
        ) const = 0;
    };
}
