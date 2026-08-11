#include "epch.h"
#include "MaterialResolver.h"

#include <Engine/Aether/System.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Material/MaterialRegistry.h>

namespace Elixir::Aether::Effect
{
    MaterialResolver::MaterialResolver(MaterialRegistry& registry)
      : m_Registry(registry) {}

    bool MaterialResolver::Resolve(const System& system) const
    {
        for (const auto& emitter : system.GetEmitters())
        {
            // A caller may replace an effect-authored instance before creating a
            // SystemInstance. Do not overwrite that explicit choice.
            if (emitter->GetMaterial())
                continue;

            Ref<Material> material;

            if (const auto& desc = emitter->GetMaterialDescription())
            {
                const auto name = "Aether." + system.GetId() + "." + emitter->GetName();

                material = m_Registry.Find(name);
                if (!material)
                {
                    material = CreateMaterial(name, emitter->GetRenderMode(), *desc);
                    if (!m_Registry.Register(material))
                    {
                        EE_CORE_ERROR("Aether material '{}' could not be registered.", name)
                        return false;
                    }
                }
            }
            else
            {
                material = m_Registry.GetDefault(GetMaterialUsage(emitter->GetRenderMode()));
            }

            emitter->SetMaterial(material);
            if (!emitter->GetMaterial()) return false;
        }

        return true;
    }
}
