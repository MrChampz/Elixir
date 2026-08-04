#pragma once

#include <Engine/Material/Material.h>

namespace Elixir
{
    inline constexpr size_t DEFAULT_MATERIAL_COUNT  = (size_t)EMaterialUsage::Count;

    using DefaultMaterialArray = std::array<Ref<Material>, DEFAULT_MATERIAL_COUNT>;

    DefaultMaterialArray CreateDefaultMaterials();
}