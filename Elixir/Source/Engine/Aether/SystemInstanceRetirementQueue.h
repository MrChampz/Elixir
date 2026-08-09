#pragma once

#include <Engine/Logging/Log.h>
#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether
{
    /**
     * @brief Transfers detached system instances to the render-frame retirement path.
     *
     * Manager enqueues an instance after removing it from the active registry and
     * published submissions. During BeginFrame(), Manager drains this queue and
     * asks Renderer to retire the instance's GPU allocation.
     *
     * The queue retains a reference to each pending instance until the render path
     * accepts its retirement. Renderer performs the later fence-safe GPU release.
     *
     * @thread_safety All public methods synchronize access to pending instances.
     */
    class ELIXIR_API SystemInstanceRetirementQueue final
    {
    public:
        /**
         * @brief Adds a detached instance to the retirement handoff.
         *
         * @param instance Instance whose GPU allocation must be retired.
         *
         * @pre instance is not null.
         */
        void Enqueue(Ref<SystemInstance> instance)
        {
            EE_CORE_ASSERT(instance, "Aether instance retirement cannot be null.")

            const std::scoped_lock lock(m_Mutex);
            m_Pending.push_back(std::move(instance));
        }

        /**
         * @brief Removes and returns all pending instances.
         *
         * @return Instances waiting to be forwarded to Renderer.
         *
         * @note The returned instances are no longer retained by this queue.
         */
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