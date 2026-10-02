#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials
{
    /**
     * @brief Number of built-in default material entries.
     *
     * Entries are indexed by EMaterialUsage.
     */
    inline constexpr size_t DEFAULT_MATERIAL_COUNT  = (size_t)EMaterialUsage::Count;

    /**
     * @brief Stores one default material for each material usage.
     *
     * Entries are indexed by EMaterialUsage.
     */
    using DefaultMaterialArray = std::array<Ref<Material>, DEFAULT_MATERIAL_COUNT>;

    /**
     * @brief Creates the engine's built-in default materials.
     * @return Default materials indexed by their material usage.
     */
    ELIXIR_API DefaultMaterialArray CreateDefaultMaterials();
}