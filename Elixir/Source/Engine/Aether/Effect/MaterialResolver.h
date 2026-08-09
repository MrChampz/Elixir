#pragma once

namespace Elixir { class MaterialRegistry; }
namespace Elixir::Aether { class System; }

namespace Elixir::Aether::Effect
{
    // Resolves effect authoring data into emitter-owned material instances.
    class ELIXIR_API MaterialResolver final
    {
    public:
        explicit MaterialResolver(MaterialRegistry& registry);

        bool Resolve(const System& system) const;

    private:
        MaterialRegistry& m_Registry;
    };
}