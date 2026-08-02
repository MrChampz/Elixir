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

    private:
        static constexpr size_t GetSlot(const EMaterialUsage usage)
        {
            return static_cast<size_t>(usage);
        }

        std::array<Ref<const MaterialRenderProxy>, 3> m_Defaults;
    };
}