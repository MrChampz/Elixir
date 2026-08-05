#include "epch.h"
#include "MaterialRegistry.h"

#include <Engine/Material/MaterialCompiler.h>

namespace Elixir
{
    MaterialRegistry::MaterialRegistry()
      : m_Defaults(CreateDefaultMaterials())
    {
        for (const auto& material : m_Defaults)
        {
            const auto registered = Register(material);
            EE_CORE_ASSERT(registered, "Default material names must be unique.")
        }
    }

    bool MaterialRegistry::Register(const Ref<Material>& material)
    {
        if (!material || material->GetName().empty()) return false;
        return m_Materials.emplace(material->GetName(), material).second;
    }

    Ref<Material> MaterialRegistry::Find(const std::string_view name) const
    {
        const auto found = m_Materials.find(std::string(name));
        return found != m_Materials.end() ? found->second : nullptr;
    }

    const Ref<Material>& MaterialRegistry::GetDefault(const EMaterialUsage usage) const
    {
        return m_Defaults[GetDefaultSlot(usage)];
    }

    size_t MaterialRegistry::GetDefaultSlot(EMaterialUsage usage)
    {
        const auto slot = (size_t)usage;
        EE_CORE_ASSERT(slot < DEFAULT_MATERIAL_COUNT, "Unsupported default material usage.")
        return slot;
    }
}
