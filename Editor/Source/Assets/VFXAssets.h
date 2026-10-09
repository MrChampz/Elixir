#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace EditorAssets
{
    /** @brief Lists effect files and reports filesystem errors encountered while scanning. */
    struct SVFXAssets
    {
        /** @brief Paths to JSON effects, sorted by their path within Assets/VFX. */
        std::vector<std::filesystem::path> Files;
        /** @brief Empty on success; otherwise describes why the scan could not finish. */
        std::string Error;
    };

    /** @brief Finds JSON effect assets recursively under the project's Assets/VFX directory. */
    SVFXAssets FindVFXAssets(const std::filesystem::path& projectRoot);
}
