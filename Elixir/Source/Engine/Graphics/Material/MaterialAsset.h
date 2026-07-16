#pragma once

#include <Engine/Graphics/Material/Material.h>

#include <filesystem>

namespace Elixir
{
    // Persists the two author-facing material assets: a Material owns its node graph
    // and defaults, while a MaterialInstance references the parent and stores overrides.
    // The graph travels as the Material's document, so nothing here needs the editor.
    class ELIXIR_API MaterialAsset
    {
      public:
        static bool SaveMaterial(const std::filesystem::path& path, const Material& material);
        static Ref<Material> LoadMaterial(const std::filesystem::path& path);

        static bool SaveInstance(const std::filesystem::path& path, const MaterialInstance& instance,
            const std::filesystem::path& parentPath);
        static Ref<MaterialInstance> LoadInstance(const std::filesystem::path& path);
    };
}
