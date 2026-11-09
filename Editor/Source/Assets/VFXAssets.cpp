#include "VFXAssets.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace EditorAssets
{
    SVFXAssets FindVFXAssets(const std::filesystem::path& projectRoot)
    {
        SVFXAssets result;
        const auto directory = projectRoot / "Assets" / "VFX";
        std::error_code error;
        std::filesystem::recursive_directory_iterator entry(directory, error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && entry != end)
        {
            auto extension = entry->path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            if (extension == ".json" && entry->is_regular_file(error))
                result.Files.push_back(entry->path());
            if (!error)
                entry.increment(error);
        }

        std::sort(result.Files.begin(), result.Files.end());
        if (error)
            result.Error = "Could not scan Assets/VFX: " + error.message();
        return result;
    }
}
