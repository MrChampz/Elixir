#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace EditorAssets
{
    /** Returns whether an asset uses a supported mesh document format. */
    inline bool IsMeshAsset(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return extension == ".gltf" || extension == ".glb";
    }
}
