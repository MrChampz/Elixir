#pragma once

namespace Elixir
{
    class MaterialRegistry;
}

namespace Elixir::Aether
{
    class System;

    // Resolves effect authoring data into emitter-owned material instances.
    class ELIXIR_API EffectMaterialResolver final
    {
    public:
        explicit EffectMaterialResolver(MaterialRegistry& registry);

        bool Resolve(System& system) const;

    private:
        MaterialRegistry& m_Registry;
    };
}