#pragma once

#include <Engine/Materials/DefaultMaterials.h>

namespace Elixir::Materials
{

    /**
     * @brief Stores application-owned material assets and default materials.
     */
    class ELIXIR_API MaterialRegistry final
    {
    public:
        /** @brief Creates the registry and registers all default materials. */
        MaterialRegistry();

        /**
         * @brief Registers a material by its name.
         * @param material Material to register.
         * @return True when the material has a valid, unique name and was registered.
         */
        bool Register(const Ref<Material>& material);

        /**
         * @brief Finds a registered material by name.
         * @param name Material name.
         * @return The registered material, or null when no material has that name.
         */
        Ref<Material> Find(std::string_view name) const;

        /**
         * @brief Gets the default material for a usage category.
         * @param usage Required material usage.
         * @return The default material for the requested usage.
         */
        const Ref<Material>& GetDefault(EMaterialUsage usage) const;

    private:
        /** @brief Converts a material usage value into a default-material array slot. */
        static size_t GetDefaultSlot(EMaterialUsage usage);

        DefaultMaterialArray m_Defaults;
        std::unordered_map<std::string, Ref<Material>> m_Materials;
    };
}
