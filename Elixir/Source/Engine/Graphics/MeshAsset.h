#pragma once

#include <Engine/Core/Core.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Elixir
{
    struct SMeshMaterialSlot
    {
        // The source material name identifies the slot; Slot is the index it had when
        // written, and only resolves entries whose name is absent or no longer present.
        uint32_t Slot = 0;
        std::string Name;
        std::filesystem::path Material;
    };

    struct SMeshAssetDescription
    {
        std::filesystem::path Source;
        std::vector<SMeshMaterialSlot> Materials;
    };

    // Native mesh metadata around an imported source such as glTF. Material slots
    // belong here; unlisted slots retain the defaults authored in the source file.
    class ELIXIR_API MeshAsset
    {
      public:
        static bool Save(const std::filesystem::path& path, const SMeshAssetDescription& description);
        static std::optional<SMeshAssetDescription> Load(const std::filesystem::path& path);
    };
}
