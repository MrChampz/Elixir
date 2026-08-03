#pragma once

#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir
{
    // Owns compiled built-in particle material proxies. This is an asset
    // service: MaterialSystem never chooses a fallback while rendering.
    class ELIXIR_API ParticleMaterialLibrary final
    {
    public:
        explicit ParticleMaterialLibrary(const ShaderLoader* shaderLoader);

        const Ref<const MaterialRenderProxy>& GetDefault(EMaterialUsage usage) const;

        Ref<const MaterialRenderProxy> GetDefaultSprite(const Ref<Texture>& texture);

    private:
        struct SDefaultMaterial
        {
            Ref<Material> Source;
            Ref<const SCompiledMaterial> Compiled;
            Ref<const MaterialRenderProxy> Proxy;
        };

        static constexpr size_t GetSlot(const EMaterialUsage usage)
        {
            return static_cast<size_t>(usage);
        }

        std::array<SDefaultMaterial, 3> m_Defaults;
        std::unordered_map<const Texture*, Ref<const MaterialRenderProxy>> m_SpriteProxies;
    };
}