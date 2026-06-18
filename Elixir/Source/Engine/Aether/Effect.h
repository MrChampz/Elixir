#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    Ref<System> LoadEffectFile(const std::filesystem::path& filepath);
}