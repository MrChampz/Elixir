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

            m_Defaults[GetSlot(usage)] = { source, compiled.Material, proxy };
        }
    }

    const Ref<const MaterialRenderProxy>& ParticleMaterialLibrary::GetDefault(
        const EMaterialUsage usage
    ) const
    {
        const auto& material = m_Defaults[GetSlot(usage)].Proxy;
        EE_CORE_ASSERT(
            material,
            "Particle material library default is unavailable."
        )
        return material;
    }

    Ref<const MaterialRenderProxy> ParticleMaterialLibrary::GetDefaultSprite(
        const Ref<Texture>& texture
    )
    {
        if (!texture) return GetDefault(EMaterialUsage::ParticleSprite);

        const auto found = m_SpriteProxies.find(texture.get());
        if (found != m_SpriteProxies.end())
            return found->second;

        const auto& source = m_Defaults[GetSlot(EMaterialUsage::ParticleSprite)];
        const auto instance = CreateRef<MaterialInstance>(source.Source);

        const bool result = instance->SetTexture(
            std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER),
            texture
        );
        EE_CORE_ASSERT(result, "Default Sprite material parameter is unavailable.")

        const auto proxy = instance->CreateRenderProxy(source.Compiled);
        EE_CORE_ASSERT(proxy, "Default Sprite material proxy creation failed.")

        m_SpriteProxies.emplace(texture.get(), proxy);
        return proxy;
    }
}
