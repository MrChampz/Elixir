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
        friend class FrameSubmissionPublisher;
    public:
        bool Submit(const SystemInstance& instance)
        {
            if (m_IsSealed) return false;

            const auto [_, inserted] = m_InstanceIds.insert(instance.GetId());
            if (!inserted) return false;

            m_Snapshots.push_back(instance.CaptureSnapshot());
            return true;
        }

        // Drop a captured state before the manager retires its GPU allocation.
        bool Remove(const SystemInstance& instance)
        {
            if (m_IsSealed) return false;

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

        Ref<const FrameSubmission> Without(const UUID& instanceId) const
        {
            auto copy = CreateRef<FrameSubmission>();
            copy->m_Snapshots = m_Snapshots;
            copy->m_InstanceIds = m_InstanceIds;

            std::erase_if(copy->m_Snapshots, [&instanceId](const auto& snapshot)
            {
                return snapshot->GetId() == instanceId;
            });

            copy->m_InstanceIds.erase(instanceId);
            copy->m_IsSealed = true;
            return copy;
        }

        bool IsSealed() const { return m_IsSealed; }

        bool IsEmpty() const { return m_Snapshots.empty(); }

        size_t GetInstanceCount() const { return m_Snapshots.size(); }

        const std::vector<Ref<const SystemInstanceSnapshot>>& GetSnapshots() const
        {
            return m_Snapshots;
        }

    private:
        void Seal() { m_IsSealed = true; }

        bool m_IsSealed = false;
        std::vector<Ref<const SystemInstanceSnapshot>> m_Snapshots;
        std::unordered_set<UUID> m_InstanceIds;
    };

    // Short synchronized handoff between the submission producer and renderer.
    class ELIXIR_API FrameSubmissionPublisher final
    {
    public:
        void Publish(Ref<FrameSubmission> submission)
        {
            EE_CORE_ASSERT(submission, "Aether frame submission cannot be null.")
            submission->Seal();

            const std::scoped_lock lock(m_Mutex);
            m_Published = std::move(submission);
        }

        Ref<const FrameSubmission> Acquire()
        {
            const std::scoped_lock lock(m_Mutex);
            return m_Published;
        }

        void Remove(const UUID& instanceId)
        {
            const std::scoped_lock lock(m_Mutex);

            if (m_Published)
                m_Published = m_Published->Without(instanceId);
        }

    private:
        mutable std::mutex m_Mutex;
        Ref<const FrameSubmission> m_Published;
    };
}