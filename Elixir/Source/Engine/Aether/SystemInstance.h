#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    // Runtime identity and immutable compiled payload selection.
    // GPU allocations belong to Renderer::ParticleResourcePool, never here.
    class ELIXIR_API SystemInstance final
    {
    public:
        explicit SystemInstance(Ref<const SCompiledSystem> compiledSystem);

        const UUID& GetId() const { return m_Id; }
        uint32_t GetRevision() const { return m_Revision; }
        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }

        void SetCompiledSystem(Ref<const SCompiledSystem> compiledSystem);

    private:
        UUID m_Id;
        uint32_t m_Revision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;
    };
}