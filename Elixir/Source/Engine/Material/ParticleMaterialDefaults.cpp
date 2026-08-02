#include "epch.h"
#include "ParticleMaterialDefaults.h"

namespace Elixir
{
    namespace
    {
        std::string GetDefaultParticleMaterialName(const EMaterialUsage usage)
        {
            switch (usage)
            {
                case EMaterialUsage::ParticleSprite:
                    return "Engine.DefaultParticleSprite";
                case EMaterialUsage::ParticleRibbon:
                    return "Engine.DefaultParticleRibbon";
                case EMaterialUsage::ParticleMesh:
                    return "Engine.DefaultParticleMesh";
            }

            return "Engine.DefaultParticle";
        }
    }

    Ref<Material> CreateDefaultParticleMaterial(const EMaterialUsage usage)
    {
        const auto name = GetDefaultParticleMaterialName(usage);
        const auto material = CreateRef<Material>(name);

        const bool usageWasEnabled = material->SetUsage(usage, true);
        EE_CORE_ASSERT(
            usageWasEnabled,
            "A default particle material must enable its particle usage."
        )

        return material;
    }
}
