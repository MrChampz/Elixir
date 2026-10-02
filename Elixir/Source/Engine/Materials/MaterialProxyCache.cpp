#include "epch.h"
#include "MaterialProxyCache.h"

namespace Elixir::Materials
{
    MaterialProxyCache::MaterialProxyCache(MaterialResolver& resolver)
      : m_Resolver(resolver) {}

    Ref<const MaterialRenderProxy> MaterialProxyCache::Resolve(
        const Ref<MaterialInstance>& instance
    )
    {
        if (!instance || !instance->GetParent())
            return nullptr;

        const auto parent = instance->GetParent();
        const auto found = m_Entries.find(instance.get());

        if (found != m_Entries.end())
        {
            const auto cachedInstance = found->second.Instance.lock();
            const auto matches =
                cachedInstance.get() == instance.get() &&
                found->second.Parent == parent.get() &&
                found->second.InstanceRevision == instance->GetRevision() &&
                found->second.MaterialRevision == parent->GetRevision();

            if (matches)
                return found->second.Proxy;

            m_Entries.erase(found);
        }

        const auto proxy = m_Resolver.Resolve(instance);
        m_Entries.emplace(instance.get(), SEntry{
            .Instance = instance,
            .Proxy = proxy,
            .Parent = parent.get(),
            .InstanceRevision = instance->GetRevision(),
            .MaterialRevision = parent->GetRevision(),
        });

        return proxy;
    }

    void MaterialProxyCache::PruneExpired()
    {
        std::erase_if(m_Entries, [](const auto& entry)
        {
            return entry.second.Instance.expired();
        });
    }
}