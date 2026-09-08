#pragma once

#include <Engine/Mesh/StaticMeshLoader.h>

namespace Elixir
{
    class GraphicsContext;

    /**
     * @brief Loads static mesh files through registered format loaders.
     *
     * The registry installs the glTF loader by default. Applications can add another
     * format loader or explicitly replace the implementation for an existing format.
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

        /** @brief Release registered loaders and the current graphics context. */
        static void Shutdown();

        /**
         * @brief Register a loader for a previously unsupported source format.
         * @param loader Loader to register.
         * @return False when loader is null or its format is already registered.
         */
        static bool RegisterLoader(Scope<StaticMeshLoader> loader);

        /**
         * @brief Replace the loader registered for one source format.
         * @param loader Loader that becomes responsible for its declared format.
         * @return False when loader is null.
         */
        static bool ReplaceLoader(Scope<StaticMeshLoader> loader);

        /**
         * @brief Import every static mesh defined by one supported source file.
         * @param path Local source file.
         * @return Imported meshes and diagnostics.
         */
        static SStaticMeshLoadResult Load(const std::filesystem::path& path);

    private:
        static std::optional<EStaticMeshFormat> InferFormat(
            const std::filesystem::path& path
        );
    };
}