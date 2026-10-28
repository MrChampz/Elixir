#pragma once

#include <Engine/Materials/Material.h>
#include <filesystem>
#include <string_view>

namespace fastgltf { class Asset; }

namespace Elixir
{
    class GraphicsContext;

    /**
     * @brief Import Surface materials with the default material at slot zero.
     * @param context Graphics context used to create material textures.
     * @param asset Parsed glTF asset that supplies material and image data.
     * @param directory Asset directory used to resolve and check external texture URIs.
     * @param name Asset name used to label imported resources.
     */
    std::vector<Ref<Materials::Material>> LoadGLTFMaterials(
        const GraphicsContext& context,
        fastgltf::Asset& asset,
        const std::filesystem::path& directory,
        std::string_view name
    );
}
