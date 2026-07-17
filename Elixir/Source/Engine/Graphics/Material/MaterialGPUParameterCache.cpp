#include "epch.h"
#include "MaterialGPUParameterCache.h"

namespace Elixir
{
    MaterialGPUParameterCache::SUpdate MaterialGPUParameterCache::Update(ProxyList proxies)
    {
        SUpdate update;
        update.LayoutChanged = proxies.size() != m_Proxies.size();
        update.DirtySlots.reserve(proxies.size());
        for (uint32_t slot = 0; slot < proxies.size(); ++slot)
        {
            if (slot >= m_Proxies.size() || proxies[slot] != m_Proxies[slot])
                update.DirtySlots.push_back(slot);
        }
        m_Proxies = std::move(proxies);
        return update;
    }
}
