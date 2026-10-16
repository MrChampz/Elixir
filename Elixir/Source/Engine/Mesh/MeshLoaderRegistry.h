#pragma once

#include <Engine/Mesh/MeshLoader.h>

namespace Elixir
{
    class GeometryPool;

    /** @brief Creates static and skeletal meshes through one registered source loader. */
    class ELIXIR_API MeshLoaderRegistry final
    {
    public:
        MeshLoaderRegistry() = delete;

        /** @brief Bind one graphics context and install the default loader when none is registered. */
        static void Initialize(const GraphicsContext& context);

        /** @brief Release the loader and geometry pool before destroying the graphics context. */
        static void Shutdown();

        /** @brief Register one loader; return false for null or when a loader is already registered. */
        static bool RegisterLoader(Scope<MeshLoader> loader);

        /** @brief Replace the source loader; return false for null. */
        static bool ReplaceLoader(Scope<MeshLoader> loader);

        /** @brief Import and upload a complete static mesh, or return no result on failure. */
        static std::optional<Ref<StaticMesh>> LoadStaticMesh(const std::filesystem::path& path);

        /** @brief Import a complete skeletal mesh, optionally reusing a compatible skeleton. */
        static std::optional<Ref<SkeletalMesh>> LoadSkeletalMesh(
            const std::filesystem::path& path,
            Ref<const Skeleton> skeleton = nullptr
        );

        /** @brief Get the static geometry pool, or null before initialization. */
        static const GeometryPool* GetGeometryPool();
    };
}
