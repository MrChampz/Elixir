#pragma once

#include <Engine/Graphics/Material/Material.h>

#include <filesystem>

namespace Elixir
{
    class MaterialGraphEditor;

    // Persists the two author-facing material assets: a Material owns its node graph
    // and defaults, while a MaterialInstance references the parent and stores overrides.
    class ELIXIR_API MaterialAsset
    {
      public:
        static bool SaveMaterial(const std::filesystem::path& path, const Material& material,
            const MaterialGraphEditor& editor);
        static Ref<Material> LoadMaterial(const std::filesystem::path& path,
            MaterialGraphEditor* editor = nullptr);

        static bool SaveInstance(const std::filesystem::path& path, const MaterialInstance& instance,
            const std::filesystem::path& parentPath);
        static Ref<MaterialInstance> LoadInstance(const std::filesystem::path& path,
            MaterialGraphEditor* parentEditor = nullptr);
    };
}
