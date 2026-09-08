#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    class GraphicsContext;

    /** @brief Identifies a source format supported by a static mesh loader. */
    enum class EStaticMeshFormat : uint8_t
    {
        GLTF,
    };

    /** @brief Supplies the engine state and source file for one mesh import. */
    struct SStaticMeshLoadRequest
    {
        const GraphicsContext* GraphicsContext = nullptr;
        std::filesystem::path Path;
    };

    /** @brief Marks the severity of one static mesh import diagnostic. */
    enum class EStaticMeshLoadDiagnosticSeverity : uint8_t
    {
        Warning, Error,
    };

    /** @brief Reports an import condition without exposing a parser implementation. */
    struct SStaticMeshLoadDiagnostic
    {
        EStaticMeshLoadDiagnosticSeverity Severity = EStaticMeshLoadDiagnosticSeverity::Error;
        std::string Message;
    };

    /** @brief Contains all static meshes produced from one source file. */
    struct SStaticMeshLoadResult
    {
        /** Imported meshes, one for each valid source mesh. */
        std::vector<Ref<StaticMesh>> Meshes;

        /** Warnings and errors produced while importing. */
        std::vector<SStaticMeshLoadDiagnostic> Diagnostics;

        /** Check whether the import produced an error. */
        bool HasErrors() const
        {
            return std::ranges::any_of(Diagnostics, [](const auto& diagnostic)
            {
                return diagnostic.Severity == EStaticMeshLoadDiagnosticSeverity::Error;
            });
        }
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

        /** Get the source format accepted by this loader. */
        virtual EStaticMeshFormat GetFormat() const = 0;

        /**
         * @brief Import all valid static meshes from one source file.
         * @param request Source file and graphics context.
         * @return Imported meshes and diagnostics.
         */
        virtual SStaticMeshLoadResult Load(const SStaticMeshLoadRequest& request) const = 0;
    };
}