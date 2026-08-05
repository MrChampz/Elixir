#include "epch.h"
#include "EffectMaterialResolver.h"

#include <Engine/Aether/System.h>
#include <Engine/Aether/ParticleMaterialFactory.h>
#include <Engine/Material/MaterialRegistry.h>

#include "spdlog/fmt/bundled/base.h"

namespace Elixir::Aether
{
    EffectMaterialResolver::EffectMaterialResolver(MaterialRegistry& registry)
      : m_Registry(registry) {}

    bool EffectMaterialResolver::Resolve(System& system) const
    {
        for (const auto& emitter : system.m_Emitters)
        {
            Ref<Material> material;

            if (const auto& definition = emitter->GetMaterialDefinition())
            {
                const auto name = "Aether." + system.m_UUID.ToString() + "." + emitter->GetName();

                material = m_Registry.Find(name);
                if (!material)
                {
                    material = CreateParticleMaterial(name, emitter->GetRenderMode(), *definition);
                    if (!m_Registry.Register(material))
                    {
                        EE_CORE_ERROR("Aether material '{}' could not be registered.", name)
                        return false;
                    }
                }
            }
            else
            {
                material = m_Registry.GetDefault(GetParticleMaterialUsage(emitter->GetRenderMode()));
            }

            emitter->SetMaterial(material);
            if (!emitter->GetMaterial()) return false;
        }

        return true;
    }
}
