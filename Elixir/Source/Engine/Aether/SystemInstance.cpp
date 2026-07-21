#include "epch.h"
#include "SystemInstance.h"

namespace Elixir::Aether
{
    SystemInstance::SystemInstance(Ref<const SCompiledSystem> compiledSystem)
        : m_CompiledSystem(std::move(compiledSystem))
    {
        EE_CORE_ASSERT(m_CompiledSystem, "SystemInstance requires a compiled system.")
    }

    void SystemInstance::SetCompiledSystem(Ref<const SCompiledSystem> compiledSystem)
    {
        EE_CORE_ASSERT(compiledSystem, "SystemInstance requires a compiled system.")

        if (m_CompiledSystem == compiledSystem)
            return;

        m_CompiledSystem = std::move(compiledSystem);
        ++m_Revision;
    }
}
