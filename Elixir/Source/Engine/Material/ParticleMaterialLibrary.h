#pragma once

#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Material/ParticleMaterialDescription.h>
#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir
{
    // Owns compiled particle material proxies. This is an asset service:
    // MaterialSystem never chooses or interprets a material while rendering.
    class ELIXIR_API ParticleMaterialLibrary final
    {
    public:
        explicit ParticleMaterialLibrary(const ShaderLoader* shaderLoader);

        Ref<const MaterialRenderProxy> Create(const SParticleMaterialDescription& desc);

    private:
        struct SMaterialKey
        {
            EMaterialUsage Usage = EMaterialUsage::ParticleSprite;
            glm::vec3 BaseColor{ 1.0f };
            float Opacity = 1.0f;
            glm::vec3 Emissive{ 0.0f };
            const Texture* TextureIdentity = nullptr;

            bool operator==(const SMaterialKey&) const = default;
        };

        struct SMaterialKeyHasher
        {
            size_t operator()(const SMaterialKey& key) const;
        };

        const ShaderLoader* m_ShaderLoader = nullptr;
        std::unordered_map<SMaterialKey, Ref<const MaterialRenderProxy>, SMaterialKeyHasher> m_Proxies;
    };
}