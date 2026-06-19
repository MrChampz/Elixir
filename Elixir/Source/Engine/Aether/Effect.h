#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    // Loads an Aether effect asset (JSON) into a System.
    // Returns nullptr if the file cannot be opened or the asset is malformed;
    // the cause is reported through the core logger.
    Ref<System> LoadEffectFile(const std::filesystem::path& filepath);
}