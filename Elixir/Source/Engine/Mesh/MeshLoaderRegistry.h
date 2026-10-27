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

        /**
         * @brief Bind a graphics context and install the default loader when none is set.
         * @param context Graphics context that owns uploaded geometry and textures.
         * @pre Shut down the registry before switching to another context.
         */
        static void Initialize(const GraphicsContext& context);

        /**
         * @brief Release the loader and geometry pool before destroying the graphics context.
         */
        static void Shutdown();

        /**
         * @brief Install a loader when none is registered.
         * @param loader Loader whose ownership is transferred to the registry.
         * @return False for null or when a loader is already registered.
         */
        static bool RegisterLoader(Scope<MeshLoader> loader);

        /**
         * @brief Replace the registered source loader.
         * @param loader Replacement loader whose ownership is transferred to the registry.
         * @return False for null; the registered loader then remains unchanged.
         */
        static bool ReplaceLoader(Scope<MeshLoader> loader);

        /**
         * @brief Import and upload a complete static mesh.
         * @param path Source file to import.
         * @return The mesh, or no value if initialization, import, upload, or mesh creation
         * fails.
         */
        static std::optional<Ref<StaticMesh>> LoadStaticMesh(const std::filesystem::path& path);

        /**
         * @brief Import and validate a complete skeletal mesh.
         * @param path Source file to import.
         * @param skeleton Optional compatible skeleton to share with the imported mesh.
         * @return The mesh, or no value if initialization, import, or mesh creation fails.
         */
        static std::optional<Ref<SkeletalMesh>> LoadSkeletalMesh(
            const std::filesystem::path& path,
            Ref<const Skeleton> skeleton = nullptr
        );

        /** @brief Get the static geometry pool, or null before initialization. */
        static const GeometryPool* GetGeometryPool();
    };
}
