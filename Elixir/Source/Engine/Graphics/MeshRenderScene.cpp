#include "epch.h"
#include "MeshRenderScene.h"

namespace Elixir
{
    void MeshRenderScene::QueueUpdate(Ref<const MeshSceneProxy> proxy)
    {
        if (!proxy || !proxy->GetHandle().IsValid())
            return;
        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_Pending.push_back(
            { EOperation::Update, proxy->GetHandle(), std::move(proxy) });
    }

    void MeshRenderScene::QueueRemove(const SModelRenderHandle handle)
    {
        if (!handle.IsValid())
            return;
        std::lock_guard<std::mutex> lock(m_PendingMutex);
        m_Pending.push_back({ EOperation::Remove, handle, nullptr });
    }

    void MeshRenderScene::RemoveActive(const SModelRenderHandle handle,
        std::vector<SModelRenderHandle>& removed)
    {
        const auto found = m_ProxyIndices.find(handle);
        if (found == m_ProxyIndices.end())
            return;

        const size_t index = found->second;
        m_Proxies.erase(m_Proxies.begin() + (ptrdiff_t)index);
        m_ProxyIndices.erase(found);
        for (size_t shifted = index; shifted < m_Proxies.size(); ++shifted)
            m_ProxyIndices[m_Proxies[shifted]->GetHandle()] = shifted;

        if (std::find(removed.begin(), removed.end(), handle) == removed.end())
            removed.push_back(handle);
    }

    std::vector<SModelRenderHandle> MeshRenderScene::ApplyPending()
    {
        std::vector<SPendingOperation> pending;
        {
            std::lock_guard<std::mutex> lock(m_PendingMutex);
            pending.swap(m_Pending);
        }

        std::vector<SModelRenderHandle> removed;
        for (SPendingOperation& operation : pending)
        {
            if (operation.Operation == EOperation::Remove)
            {
                RemoveActive(operation.Handle, removed);
                continue;
            }

            if (!operation.Proxy || !operation.Proxy->IsOwnerAlive())
            {
                RemoveActive(operation.Handle, removed);
                continue;
            }

            if (const auto existing = m_ProxyIndices.find(operation.Handle);
                existing != m_ProxyIndices.end())
            {
                m_Proxies[existing->second] = std::move(operation.Proxy);
            }
            else
            {
                m_ProxyIndices[operation.Handle] = m_Proxies.size();
                m_Proxies.push_back(std::move(operation.Proxy));
            }
            std::erase(removed, operation.Handle);
        }

        // A proxy owns all GPU-facing data required to finish the current frame. Its
        // weak token is inspected only at frame boundaries to schedule safe retirement.
        std::vector<SModelRenderHandle> expired;
        for (const Ref<const MeshSceneProxy>& proxy : m_Proxies)
        {
            if (!proxy->IsOwnerAlive())
                expired.push_back(proxy->GetHandle());
        }
        for (const SModelRenderHandle handle : expired)
            RemoveActive(handle, removed);
        return removed;
    }
}
