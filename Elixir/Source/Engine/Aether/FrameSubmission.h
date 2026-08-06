#pragma once

#include <unordered_set>
#include <vector>

#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether
{
    // Immutable system instance states selected for one rendering frame.
    // Submit() captures the state exactly once; an instance can be selected
    // at most once per frame.
    class ELIXIR_API FrameSubmission final
    {
    public:
        bool Submit(const SystemInstance& instance)
        {
            const auto [_, inserted] = m_InstanceIds.insert(instance.GetId());
            if (!inserted) return false;

            m_Snapshots.push_back(instance.CaptureSnapshot());
            return true;
        }

        // Drop a captured state before the manager retires its GPU allocation.
        bool Remove(const SystemInstance& instance)
        {
            const auto found = m_InstanceIds.find(instance.GetId());
            if (found == m_InstanceIds.end()) return false;

            std::erase_if(m_Snapshots, [&instance](const auto& snapshot)
            {
                return snapshot->GetId() == instance.GetId();
            });
            m_InstanceIds.erase(found);
            return true;
        }

        void Reset()
        {
            m_Snapshots.clear();
            m_InstanceIds.clear();
        }

        bool IsEmpty() const { return m_Snapshots.empty(); }

        size_t GetInstanceCount() const { return m_Snapshots.size(); }

        const std::vector<Ref<const SystemInstanceSnapshot>>& GetSnapshots() const
        {
            return m_Snapshots;
        }

    private:
        std::vector<Ref<const SystemInstanceSnapshot>> m_Snapshots;
        std::unordered_set<UUID> m_InstanceIds;
    };
}