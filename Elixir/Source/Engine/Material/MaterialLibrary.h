#pragma once

#include <Engine/Material/DefaultMaterials.h>

namespace Elixir
{
    class ShaderLoader;
    struct SCompiledMaterial;

    // Application-owned registry and compiled-material cache.
    class ELIXIR_API MaterialLibrary final
    {
    public:
        explicit MaterialLibrary(const ShaderLoader* shaderLoader);

        bool Register(const Ref<Material>& material);
        Ref<Material> Find(std::string_view name) const;
        const Ref<Material>& GetDefault(EMaterialUsage usage) const;

        Ref<const SCompiledMaterial> GetCompiledMaterial(const Ref<Material>& material);

    private:
        struct SCompiledEntry
        {
            uint32_t Revision = 0;
            Ref<const SCompiledMaterial> Material;
        };

        static size_t GetDefaultSlot(EMaterialUsage usage);

        const ShaderLoader* m_ShaderLoader = nullptr;
        DefaultMaterialArray m_Defaults;
        std::unordered_map<std::string, Ref<Material>> m_Materials;
        std::unordered_map<const Material*, SCompiledEntry> m_CompiledMaterials;
    };
}