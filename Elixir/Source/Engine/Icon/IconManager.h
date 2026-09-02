#pragma once

#include <Engine/Icon/Icon.h>

namespace Elixir
{
    class GraphicsContext;
}

namespace Elixir
{
    /**
     * @brief Loads icons through registered format loaders.
     *
     * Initialize the manager once after the graphics context exists. It registers the SVG
     * loader by default; applications can add or explicitly replace other format loaders.
     */
    class ELIXIR_API IconManager final
    {
      public:
        IconManager() = delete;

        /**
         * @brief Initialize icon loading for one graphics context.
         * @param context Graphics context that owns raster textures.
         */
        static void Initialize(const GraphicsContext* context);

        /** @brief Release registered loaders and cached manager state. */
        static void Shutdown();

        /**
         * @brief Register a loader for a previously unsupported icon format.
         * @param loader Loader to register.
         * @return False when that format already has a loader.
         */
        static bool RegisterLoader(Scope<IconLoader> loader);

        /**
         * @brief Replace the loader registered for one icon format.
         * @param loader Loader that becomes responsible for its declared format.
         * @return False when loader is null.
         */
        static bool ReplaceLoader(Scope<IconLoader> loader);

        /**
         * @brief Import an icon file based on its extension.
         * @param path Local icon file.
         * @return Icon, or nullptr when no loader accepts the file.
         */
        static Ref<::Icon> Load(const std::filesystem::path& path);

      private:
        static std::optional<EIconFormat> InferFormat(const std::filesystem::path& path);
    };
}
