#pragma once

#include <Engine/Logging/Log.h>
#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether
{
    // Cross-shared handoff for instances whose GPU allocations must be retired.
    class ELIXIR_API SystemInstanceRetirementQueue final
    {
    public:
        void Enqueue(Ref<SystemInstance> instance)
        {
            EE_CORE_ASSERT(instance, "Aether instance retirement cannot be null.")

            const std::scoped_lock lock(m_Mutex);
            m_Pending.push_back(std::move(instance));
        }

        std::vector<Ref<SystemInstance>> Drain()
        {
            const std::scoped_lock lock(m_Mutex);
            return std::exchange(m_Pending, {});
        }

    private:
        std::mutex m_Mutex;
        std::vector<Ref<SystemInstance>> m_Pending;
    };
}