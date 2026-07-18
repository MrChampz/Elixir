#pragma once

#include <Engine/Graphics/MeshSceneProxy.h>

#include <mutex>
#include <unordered_map>

namespace Elixir
{
    // Thread-safe producer queue plus render-thread-owned proxy set. Producers never
    // mutate the active scene; ApplyPending publishes a complete frame-boundary view.
    class ELIXIR_API MeshRenderScene
    {
      public:
        void QueueUpdate(Ref<const MeshSceneProxy> proxy);
        void QueueRemove(SModelRenderHandle handle);

        // Must be called by the render thread. Returns every handle removed explicitly
        // or because its authoring lifetime ended, so GPU state can be retired safely.
        [[nodiscard]] std::vector<SModelRenderHandle> ApplyPending();

        [[nodiscard]] const std::vector<Ref<const MeshSceneProxy>>& GetProxies() const
        {
            return m_Proxies;
        }

      private:
        enum class EOperation : uint8_t
        {
            Update,
            Remove
        };

        struct SPendingOperation
        {
            EOperation Operation = EOperation::Update;
            SModelRenderHandle Handle;
            Ref<const MeshSceneProxy> Proxy;
        };

        void RemoveActive(SModelRenderHandle handle,
            std::vector<SModelRenderHandle>& removed);

        std::mutex m_PendingMutex;
        std::vector<SPendingOperation> m_Pending;

        // Only the render thread touches active proxies and their stable order.
        std::vector<Ref<const MeshSceneProxy>> m_Proxies;
        std::unordered_map<SModelRenderHandle, size_t,
            SModelRenderHandleHash> m_ProxyIndices;
    };
}
