#pragma once

#include <Engine/Graphics/Material/MaterialRenderProxy.h>

#include <cstdint>
#include <vector>

namespace Elixir
{
    // Tracks the immutable proxy identities behind one GPU material buffer. The
    // renderer uses DirtySlots to repack only changed entries while LayoutChanged
    // requests a new buffer generation even when the surviving entries are unchanged.
    class ELIXIR_API MaterialGPUParameterCache
    {
      public:
        using ProxyList = std::vector<Ref<const MaterialRenderProxy>>;

        struct SUpdate
        {
            bool LayoutChanged = false;
            std::vector<uint32_t> DirtySlots;

            [[nodiscard]] bool HasChanges() const
            {
                return LayoutChanged || !DirtySlots.empty();
            }
        };

        SUpdate Update(ProxyList proxies);
        void Clear() { m_Proxies.clear(); }

        [[nodiscard]] const ProxyList& GetProxies() const { return m_Proxies; }

      private:
        ProxyList m_Proxies;
    };
}
