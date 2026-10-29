#pragma once

#include <Engine/Core/Core.h>

#include <mutex>
#include <vector>

namespace Elixir
{
    class Animation;
    class Executor;
    class Timestep;

    /**
     * @brief Manage registered animation instances.
     *
     * Animation factories register new instances with this manager. Registrations track
     * instances without taking ownership and are removed after the instances are released.
     * Registered instances can opt out of managed updates with SetManaged(false)
     * while keeping their registration.
     */
    class ELIXIR_API AnimationManager final
    {
    public:
        /** @brief Get the manager shared by animation factories and the application. */
        static AnimationManager& Get();

        /**
         * @brief Advance the registered animations that participate in managed updates.
         *
         * A failed animation keeps its last published frame. Other registered animations
         * may still advance.
         *
         * @param frameTime Finite nonnegative elapsed frame time.
         * @return True if every participating animation updated successfully.
         * @pre Call on the game thread, outside executor workers. Configure instances before
         * this call and consume results afterward. Do not overlap updates or change shared
         * animation resources during evaluation. Shared custom evaluators must be thread-safe.
         */
        bool Update(Timestep frameTime);

    private:
        friend class Animation;

        /**
         * @brief Create an animation registry with its update dispatcher.
         * @param executor Dispatcher that remains alive while managed updates are used.
         */
        explicit AnimationManager(Executor& executor);

        /**
         * @brief Register a new animation without taking ownership.
         * @param animation New instance registered once by Animation::Create.
         */
        void Register(const Ref<Animation>& animation);

        /** Update dispatcher supplied once during construction. */
        Executor& m_Executor;
        std::mutex m_RegistryMutex;
        std::mutex m_UpdateMutex;
        std::vector<WeakRef<Animation>> m_Animations;
    };
}
