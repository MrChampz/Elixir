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

    Ref<SystemInstance> InstanceRegistry::CreateInstance(const Ref<System>& system)
    {
        EE_CORE_ASSERT(system, "Aether requires a System asset.")
        if (!system) return nullptr;

        const std::scoped_lock lock(m_Mutex);

        auto compiled = m_CompiledSystems.find(system->GetId());
        if (compiled == m_CompiledSystems.end())
        {
            const auto result = CompileSystem(*system);
            if (!result) return nullptr;

            compiled = m_CompiledSystems.emplace(system->GetId(), result).first;
        }

        auto instance = Ref<SystemInstance>(new SystemInstance(compiled->second));
        const auto [_, inserted] = m_Instances.emplace(instance->GetKey(), instance);

        EE_CORE_ASSERT(inserted, "Aether system instance UUID must be unique.")

        return instance;
    }

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

    Ref<SystemInstance> InstanceRegistry::DetachInstance(
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

        return detached;
    }

    bool InstanceRegistry::Submit(
        FrameSubmission& submission,
        const Ref<SystemInstance>& instance
    )
    {
        const std::scoped_lock lock(m_Mutex);
        return IsManagedInstance(instance) && submission.Submit(*instance);
    }

    void InstanceRegistry::Publish(Ref<FrameSubmission> submission)
    {
        EE_CORE_ASSERT(submission, "Aether frame submission cannot be null.")

        const std::scoped_lock lock(m_Mutex);
        m_Publisher.Publish(
            std::move(submission),
            [this](const SSystemInstanceKey& key)
            {
                return m_Instances.contains(key);
            }
        );
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
