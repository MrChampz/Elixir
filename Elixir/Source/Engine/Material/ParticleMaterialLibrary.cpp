#include "epch.h"
#include "ParticleMaterialLibrary.h"
#include "ParticleMaterialDefaults.h"

#include <Engine/Material/MaterialCompiler.h>
#include <Engine/Material/MaterialInstance.h>

namespace Elixir
{
    ParticleMaterialLibrary::ParticleMaterialLibrary(const ShaderLoader* shaderLoader)
        : m_ShaderLoader(shaderLoader) {}

    Ref<const MaterialRenderProxy> ParticleMaterialLibrary::Create(
        const SParticleMaterialDescription& desc
    )
    {
        const SMaterialKey key{
            .Usage = desc.Usage,
            .BaseColor = desc.BaseColor,
            .Opacity = desc.Opacity,
            .Emissive = desc.Emissive,
            .TextureIdentity = desc.Texture.get(),
        };

        const auto found = m_Proxies.find(key);
        if (found != m_Proxies.end())
            return found->second;

        const auto& source = CreateParticleMaterial(desc);
        const auto compiled = MaterialCompiler::Compile(m_ShaderLoader, *source);
        EE_CORE_ASSERT(compiled, "Particle material compilation failed: {}", compiled.Diagnostics)
        if (!compiled) return nullptr;

        const auto instance = CreateRef<MaterialInstance>(source);
        if (desc.Usage == EMaterialUsage::ParticleSprite)
        {
            const bool bound = instance->SetTexture(
                std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER),
                desc.Texture
            );
            EE_CORE_ASSERT(bound, "Particle Sprite texture parameter is unavailable.")
        }

        auto proxy = instance->CreateRenderProxy(compiled.Material);
        EE_CORE_ASSERT(proxy, "Particle material proxy creation failed.")
        if (!proxy) return nullptr;

        m_Proxies.emplace(key, proxy);
        return proxy;
    }

    size_t ParticleMaterialLibrary::SMaterialKeyHasher::operator()(const SMaterialKey& key) const
    {
        size_t hash = Hash::Hash<uint32_t>(uint32_t(key.Usage));
        const auto mix = [&hash](const size_t value)
        {
            hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        };

        mix(Hash::Hash<float>(key.BaseColor.x));
        mix(Hash::Hash<float>(key.BaseColor.y));
        mix(Hash::Hash<float>(key.BaseColor.z));
        mix(Hash::Hash<float>(key.Opacity));
        mix(Hash::Hash<float>(key.Emissive.x));
        mix(Hash::Hash<float>(key.Emissive.y));
        mix(Hash::Hash<float>(key.Emissive.z));
        mix(Hash::Hash<const Texture*>(key.TextureIdentity));
        return hash;
    }
}
