#include "epch.h"
#include "Manager.h"

#include <Engine/Aether/Effect.h>
#include <Engine/Material/MaterialResolver.h>

namespace Elixir::Aether
{
    Manager::Manager(MaterialRegistry& materialRegistry, MaterialResolver& materialResolver)
      : m_EffectMaterials(materialRegistry),
        m_MaterialResolver(materialResolver) {}

    Ref<System> Manager::LoadEffect(const std::filesystem::path& filepath) const
    {
        return LoadEffectFile(filepath);
    }

    Ref<const SCompiledSystem> Manager::Compile(System& system) const
    {
        if (!m_EffectMaterials.Resolve(system))
        {
            EE_CORE_ERROR("Could not resolve materials for Aether system '{}'.", system.GetName())
            return nullptr;
        }

        return CreateRef<SCompiledSystem>(system.Compile(m_MaterialResolver));
    }

    Scope<SystemInstance> Manager::CreateInstance(Ref<const SCompiledSystem> system) const
    {
        if (!system) return nullptr;
        return CreateScope<SystemInstance>(std::move(system));
    }
}
