#include "epch.h"
#include "MaterialLibrary.h"

#include <Engine/Material/MaterialCompiler.h>

namespace Elixir
{
    MaterialLibrary::MaterialLibrary(const ShaderLoader* shaderLoader)
      : m_ShaderLoader(shaderLoader),
        m_Defaults(CreateDefaultMaterials())
    {
        for (const auto& material : m_Defaults)
        {
            const auto registered = Register(material);
            EE_CORE_ASSERT(registered, "Default material names must be unique.")
        }
    }

    bool MaterialLibrary::Register(const Ref<Material>& material)
    {
        if (!material || material->GetName().empty()) return false;
        return m_Materials.emplace(material->GetName(), material).second;
    }

    Ref<Material> MaterialLibrary::Find(const std::string_view name) const
    {
        const auto found = m_Materials.find(std::string(name));
        return found != m_Materials.end() ? found->second : nullptr;
    }

    const Ref<Material>& MaterialLibrary::GetDefault(const EMaterialUsage usage) const
    {
        return m_Defaults[GetDefaultSlot(usage)];
    }

    Ref<const SCompiledMaterial> MaterialLibrary::GetCompiledMaterial(
        const Ref<Material>& material
    )
    {
        if (!material) return nullptr;

        auto& entry = m_CompiledMaterials[material.get()];
        if (entry.Material && entry.Revision == material->GetRevision())
            return entry.Material;

        const auto result = MaterialCompiler::Compile(m_ShaderLoader, *material);
        if (!result)
        {
            EE_CORE_ERROR(
                "Material '{}' compilation failed: {}",
                material->GetName(),
                result.Diagnostics
            )
            return nullptr;
        }

        entry.Revision = material->GetRevision();
        entry.Material = result.Material;
        return entry.Material;
    }

    size_t MaterialLibrary::GetDefaultSlot(EMaterialUsage usage)
    {
        const auto slot = (size_t)usage;
        EE_CORE_ASSERT(slot < DEFAULT_MATERIAL_COUNT, "Unsupported default material usage.")
        return slot;
    }
}
