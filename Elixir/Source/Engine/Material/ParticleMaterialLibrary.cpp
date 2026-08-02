#include "epch.h"
#include "ParticleMaterialLibrary.h"
#include "ParticleMaterialDefaults.h"

#include <Engine/Material/MaterialCompiler.h>
#include <Engine/Material/MaterialInstance.h>

namespace Elixir
{
    ParticleMaterialLibrary::ParticleMaterialLibrary(const ShaderLoader* shaderLoader)
    {
        for (const auto usage : {
            EMaterialUsage::ParticleSprite,
            EMaterialUsage::ParticleRibbon,
            EMaterialUsage::ParticleMesh,
        })
        {
            const auto source = CreateDefaultParticleMaterial(usage);
            const auto compiled = MaterialCompiler::Compile(shaderLoader, *source);

            EE_CORE_ASSERT(
                compiled,
                "Default particle material compilation failed: {}",
                compiled.Diagnostics
            )
            if (!compiled) continue;

            const auto instance = CreateRef<MaterialInstance>(source);
            const auto proxy = instance->CreateRenderProxy(compiled.Material);

            EE_CORE_ASSERT(
                proxy,
                "Default particle material render proxy creation failed."
            )
            if (!proxy) continue;

            m_Defaults[GetSlot(usage)] = proxy;
        }
    }

    const Ref<const MaterialRenderProxy>& ParticleMaterialLibrary::GetDefault(
        const EMaterialUsage usage
    ) const
    {
        const auto& material = m_Defaults[GetSlot(usage)];
        EE_CORE_ASSERT(
            material,
            "Particle material library default is unavailable."
        )
        return material;
    }
}
