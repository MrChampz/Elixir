#include "epch.h"
#include "MaterialResolver.h"

#include <Engine/Aether/System.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Materials/MaterialRegistry.h>

namespace Elixir::Aether::Effect
{
    MaterialResolver::MaterialResolver(MaterialRegistry& registry)
      : m_Registry(registry) {}

    bool MaterialResolver::Resolve(const System& system) const
    {
        for (const auto& emitter : system.GetEmitters())
        {
            Ref<Material> material;

            if (const auto& desc = emitter->GetMaterialDescription())
            {
                const auto name = "Aether." + system.GetId() + "." + emitter->GetName();

                // Preserve a caller-selected material, but recreate the material
                // previously generated for this emitter when its asset data changes.
                if (emitter->GetMaterial() && !emitter->HasResolvedMaterial())
                    continue;

                material = m_Registry.Find(name);
                const auto refreshed = CreateMaterial(name, *desc);
                if (material)
                {
                    if (!m_Registry.Replace(refreshed))
                    {
                        EE_CORE_ERROR("Aether material '{}' could not be refreshed.", name)
                        return false;
                    }

                    material = refreshed;
                }
                else
                {
                    material = std::move(refreshed);
                    if (!m_Registry.Register(material))
                    {
                        EE_CORE_ERROR("Aether material '{}' could not be registered.", name)
                        return false;
                    }
                }
            }
            else
            {
                // A caller may replace a default instance before creating a
                // SystemInstance. Do not overwrite that explicit choice.
                if (emitter->GetMaterial() && !emitter->HasResolvedMaterial())
                    continue;

                material = m_Registry.GetDefault(EMaterialUsage::Particle);
            }

            emitter->SetResolvedMaterial(material->CreateInstance());
            if (!emitter->GetMaterial()) return false;
        }

        return true;
    }
}
