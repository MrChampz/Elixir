#pragma once

#include <Engine/Aether/EffectMaterialResolver.h>
#include <Engine/Aether/SystemInstance.h>

namespace Elixir
{
    class MaterialRegistry;
    class MaterialResolver;
}

namespace Elixir::Aether
{
    // Application-scoped entry point for effect assets and their immutable
    // runtime payloads. Renderer owns GPU allocation and drawing separately.
    class ELIXIR_API Manager final
    {
    public:
        Manager(MaterialRegistry& materialRegistry, MaterialResolver& materialResolver);

        Ref<System> LoadEffect(const std::filesystem::path& filepath) const;

        Ref<const SCompiledSystem> Compile(System& system) const;

        Scope<SystemInstance> CreateInstance(Ref<const SCompiledSystem> system) const;

    private:
        EffectMaterialResolver m_EffectMaterials;
        MaterialResolver& m_MaterialResolver;
    };
}