#include "epch.h"
#include "AnimationManager.h"

#include <Engine/Animation/Animation.h>
#include <Engine/Core/Executor/Executor.h>

#include <atomic>
#include <cmath>

namespace Elixir
{
    AnimationManager& AnimationManager::Get()
    {
        static AnimationManager manager;
        return manager;
    }

    void AnimationManager::Register(const Ref<Animation>& animation)
    {
        std::lock_guard lock(m_RegistryMutex);
        m_Animations.emplace_back(animation);
    }

    bool AnimationManager::Update(const float deltaSeconds, Executor& executor)
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
        {
            EE_CORE_ERROR("Animation updates require a finite nonnegative frame duration.")
            return false;
        }

        const std::unique_lock updateLock(m_UpdateMutex, std::try_to_lock);
        if (!updateLock.owns_lock())
        {
            EE_CORE_ERROR("Animation manager updates must not overlap.")
            return false;
        }

        std::vector<Ref<Animation>> instances;
        {
            std::lock_guard lock(m_RegistryMutex);

            auto retained = m_Animations.begin();
            for (auto current = m_Animations.begin(); current != m_Animations.end(); ++current)
            {
                if (auto instance = current->lock())
                {
                    *retained++ = *current;
                    if (instance->IsManaged())
                        instances.push_back(std::move(instance));
                }
            }

            m_Animations.erase(retained, m_Animations.end());
        }

        WaitGroup completion;
        std::atomic succeeded{ true };

        for (const auto& instance : instances)
        {
            auto evaluate = [instance, deltaSeconds, &succeeded]
            {
                EE_PROFILE_ZONE_SCOPED()
                if (!instance->Update(deltaSeconds))
                    succeeded.store(false, std::memory_order_relaxed);
            };

            const auto submitted = executor.TryEnqueue(std::move(evaluate), &completion);
            if (!submitted)
            {
                EE_CORE_ERROR("Animation manager could not queue a worker job.")
                succeeded.store(false, std::memory_order_relaxed);
                break;
            }
        }

        completion.Wait();
        return succeeded.load(std::memory_order_relaxed);
    }
}
