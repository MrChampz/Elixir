#include "epch.h"
#include "InstanceRegistry.h"

#include <Engine/Material/MaterialRegistry.h>
#include <Engine/Material/MaterialResolver.h>

namespace Elixir::Aether::Runtime
{
    InstanceRegistry::InstanceRegistry(
        MaterialRegistry& materialRegistry,
        MaterialResolver& materialResolver
    ) : m_EffectMaterials(materialRegistry),
        m_MaterialResolver(materialResolver) {}

    bool InstanceRegistry::Recompile(const Ref<System>& system)
    {
        EE_CORE_ASSERT(system, "Aether requires a System asset.")
        if (!system) return false;

        const std::scoped_lock lock(m_Mutex);

        const auto compiled = CompileSystem(*system);
        if (!compiled) return false;

        m_CompiledSystems.insert_or_assign(system->GetId(), compiled);

        for (const auto& instance : m_Instances | std::views::values)
        {
            if (instance->GetSourceSystemId() == system->GetId())
                instance->ApplyCompilation(compiled);
        }

        return true;
    }

    bool InstanceRegistry::Register(const Ref<SystemInstance>& instance)
    {
        if (!instance || !instance->TryBeginSubmission())
            return false;

        const auto system = instance->GetSourceSystem();
        if (!system)
        {
            instance->CancelSubmission();
            return false;
        }

        {
            const std::scoped_lock lock(m_Mutex);

            if (m_Instances.contains(instance->GetKey()))
            {
                instance->CancelSubmission();
                EE_CORE_ASSERT(false, "Aether system instance UUID must be unique.")
                return false;
            }

            auto compiled = m_CompiledSystems.find(system->GetId());
            if (compiled == m_CompiledSystems.end())
            {
                const auto result = CompileSystem(*system);
                if (!result)
                {
                    instance->CancelSubmission();
                    return false;
                }

                compiled = m_CompiledSystems.emplace(system->GetId(), result).first;
            }

            if (!instance->Initialize(compiled->second))
            {
                instance->CancelSubmission();
                return false;
            }

            m_Instances.emplace(instance->GetKey(), instance);
            m_InstanceOrder.push_back(instance->GetKey());
        }

        return true;
    }

    Ref<SystemInstance> InstanceRegistry::Unregister(
        const Ref<SystemInstance>& instance
    )
    {
        const std::scoped_lock lock(m_Mutex);

        if (!IsManagedInstance(instance))
            return nullptr;

        const auto found = m_Instances.find(instance->GetKey());
        if (found == m_Instances.end())
            return nullptr;

        m_Publisher.Remove(*instance);

        auto detached = found->second;
        m_Instances.erase(found);
        std::erase(m_InstanceOrder, instance->GetKey());

        return detached;
    }

    void InstanceRegistry::PublishActiveInstances()
    {
        const std::scoped_lock lock(m_Mutex);

        auto submission = CreateRef<FrameSubmission>();

        for (const auto& key : m_InstanceOrder)
        {
            const auto found = m_Instances.find(key);
            EE_CORE_ASSERT(
                found != m_Instances.end(),
                "Aether active instance is not registered."
            )

            if (found != m_Instances.end())
            {
                const bool submitted = submission->Submit(*found->second);
                EE_CORE_ASSERT(
                    submitted,
                    "Aether could not capture an active system instance."
                )
            }
        }

        m_Publisher.Publish(std::move(submission));
    }

    Ref<const FrameSubmission> InstanceRegistry::AcquireSubmission()
    {
        return m_Publisher.Acquire();
    }

    Ref<const SCompiledSystem> InstanceRegistry::CompileSystem(
        const System& system
    ) const
    {
        if (!m_EffectMaterials.Resolve(system))
        {
            EE_CORE_ERROR(
                "Could not resolve materials for Aether system '{}'.",
                system.GetName()
            )
            return nullptr;
        }

        return CreateRef<SCompiledSystem>(
            system.Compile(m_MaterialResolver)
        );
    }

    bool InstanceRegistry::IsManagedInstance(
        const Ref<SystemInstance>& instance
    ) const
    {
        if (!instance) return false;

        const auto found = m_Instances.find(instance->GetKey());
        return found != m_Instances.end() && found->second.get() == instance.get();
    }
}
