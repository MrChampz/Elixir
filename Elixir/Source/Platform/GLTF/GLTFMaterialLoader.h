#pragma once

#include <Engine/Materials/Material.h>
#include <filesystem>
#include <string_view>

namespace fastgltf { class Asset; }

namespace Elixir
{
    class GraphicsContext;

    /** @brief Import Surface materials with the default material at slot zero. */
    std::vector<Ref<Materials::Material>> LoadGLTFMaterials(
        const GraphicsContext& context,
        fastgltf::Asset& asset,
        const std::filesystem::path& directory,
        std::string_view name
    );
}
