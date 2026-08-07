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

            const auto [_, inserted] = m_InstanceKeys.insert(instance.GetKey());
            if (!inserted) return false;

            m_Snapshots.push_back(instance.CaptureSnapshot());
            return true;
        }

        // Drop a captured state before the manager retires its GPU allocation.
        bool Remove(const SystemInstance& instance)
        {
            if (m_IsSealed) return false;

            const auto found = m_InstanceKeys.find(instance.GetKey());
            if (found == m_InstanceKeys.end()) return false;

            std::erase_if(m_Snapshots, [&instance](const auto& snapshot)
            {
                return snapshot->GetKey() == instance.GetKey();
            });
            m_InstanceKeys.erase(found);
            return true;
        }

        void Reset()
        {
            m_Snapshots.clear();
            m_InstanceKeys.clear();
        }

        Ref<const FrameSubmission> Without(const SSystemInstanceKey& key) const
        {
            auto copy = CreateRef<FrameSubmission>();
            copy->m_Snapshots = m_Snapshots;
            copy->m_InstanceKeys = m_InstanceKeys;

            std::erase_if(copy->m_Snapshots, [&key](const auto& snapshot)
            {
                return snapshot->GetKey() == key;
            });

            copy->m_InstanceKeys.erase(key);
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
        std::unordered_set<SSystemInstanceKey> m_InstanceKeys;
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

        void Remove(const SystemInstance& instance)
        {
            const std::scoped_lock lock(m_Mutex);

            if (m_Published)
                m_Published = m_Published->Without(instance.GetKey());
        }

    private:
        mutable std::mutex m_Mutex;
        Ref<const FrameSubmission> m_Published;
    };
}