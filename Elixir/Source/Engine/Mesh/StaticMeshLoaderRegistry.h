#pragma once

#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Mesh/StaticMeshLoader.h>

namespace Elixir
{
    class GraphicsContext;

    /**
     * @brief Creates runtime static meshes through the active loader and GeometryPool.
     *
     * The application selects the implementation during startup.
     */
    class ELIXIR_API StaticMeshLoaderRegistry final
    {
    public:
        StaticMeshLoaderRegistry() = delete;

        /**
         * @brief Initialize the registry for one graphics context.
         * @param context Graphics context used to upload imported geometry.
         */
        static void Initialize(const GraphicsContext& context);

        /** @brief Release the loader, geometry pool, and graphics context. */
        static void Shutdown();

        /**
         * @brief Register the active static mesh loader.
         * @param loader Loader to register.
         * @return False when loader is null or another loader is already active.
         */
        static bool RegisterLoader(Scope<StaticMeshLoader> loader);

        /**
         * @brief Replace the active static mesh loader.
         * @param loader Loader that becomes active.
         * @return False when loader is null.
         */
        static bool ReplaceLoader(Scope<StaticMeshLoader> loader);

        /**
         * @brief Load CPU data and create one runtime static mesh.
         * @param path Local source file.
         * @return Loaded mesh, or std::nullopt when loading fails.
         */
        static std::optional<Ref<StaticMesh>> Load(const std::filesystem::path& path);

        /** @brief Get the pool that owns static mesh GPU geometry. */
        static const GeometryPool& GetGeometryPool();
    };
}
