#pragma once

#include <Engine/Material/DefaultMaterials.h>

namespace Elixir
{

    // Application-owned registry for raw material assets and defaults.
    class ELIXIR_API MaterialRegistry final
    {
    public:
        MaterialRegistry();

        bool Register(const Ref<Material>& material);
        Ref<Material> Find(std::string_view name) const;
        const Ref<Material>& GetDefault(EMaterialUsage usage) const;

    private:
        static size_t GetDefaultSlot(EMaterialUsage usage);

        DefaultMaterialArray m_Defaults;
        std::unordered_map<std::string, Ref<Material>> m_Materials;
    };
}