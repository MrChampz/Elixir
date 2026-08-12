#pragma once

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Rendering/SystemInstanceRenderProxy.h>

namespace Elixir::Aether::Rendering
{
    /**
     * @brief Collects immutable system-instance render proxies for one frame.
     *
     * A single producer builds a submission by adding managed SystemInstance
     * objects. Submit() captures each instance once and stores only its immutable
     * SystemInstanceRenderProxy.
     *
     * Publish a completed submission through FrameSubmissionPublisher. A sealed
     * submission cannot accept or remove instances.
     *
     * @thread_safety Not synchronized. One producer must build or reset a
     * submission at a time.
     */
    class ELIXIR_API FrameSubmission final
    {
        friend class FrameSubmissionPublisher;

    public:
        /**
         * @brief Captures an instance for this frame.
         *
         * The method captures the instance's current immutable render proxy. The
         * same instance can appear only once in a submission.
         *
         * @param instance Runtime instance to capture.
         * @return True when the instance was added.
         * @return False when the instance is unregistered, duplicated, or the
         * submission is sealed.
         */
        bool Submit(const SystemInstance& instance)
        {
            if (m_IsSealed) return false;

            const auto [_, inserted] = m_InstanceKeys.insert(instance.GetKey());
            if (!inserted) return false;

            const auto snapshot = instance.CaptureSnapshot();
            if (!snapshot)
            {
                m_InstanceKeys.erase(instance.GetKey());
                return false;
            }

            m_RenderProxies.push_back(snapshot->GetRenderProxy());
            return true;
        }

        /**
         * @brief Removes a captured instance before publication.
         *
         * @param instance Runtime instance to remove.
         * @return True when the instance was removed.
         * @return False when the submission is sealed or does not contain instance.
         */
        bool Remove(const SystemInstance& instance)
        {
            if (m_IsSealed) return false;

            const auto found = m_InstanceKeys.find(instance.GetKey());
            if (found == m_InstanceKeys.end()) return false;

            std::erase_if(m_RenderProxies, [&instance](const auto& proxy)
            {
                return proxy->GetKey() == instance.GetKey();
            });

            m_InstanceKeys.erase(found);

            return true;
        }

        /**
         * @brief Clears all captured instances.
         *
         * Call this method only before publishing the submission.
         */
        void Reset()
        {
            m_RenderProxies.clear();
            m_InstanceKeys.clear();
        }

        /**
         * @brief Creates a sealed copy that excludes one internal instance key.
         *
         * @param key Internal identity of the instance to exclude.
         * @return A sealed submission that does not contain key.
         */
        Ref<const FrameSubmission> Without(const SSystemInstanceKey& key) const
        {
            auto copy = CreateRef<FrameSubmission>();
            copy->m_RenderProxies = m_RenderProxies;
            copy->m_InstanceKeys = m_InstanceKeys;

            std::erase_if(copy->m_RenderProxies, [&key](const auto& proxy)
            {
                return proxy->GetKey() == key;
            });

            copy->m_InstanceKeys.erase(key);
            copy->m_IsSealed = true;
            return copy;
        }

        /**
         * @brief Reports whether this submission is immutable.
         * @return True after a publisher seals the submission.
         */
        bool IsSealed() const { return m_IsSealed; }

        /**
         * @brief Reports whether this submission contains no instances.
         * @return True when no render proxies were captured.
         */
        bool IsEmpty() const { return m_RenderProxies.empty(); }

        /**
         * @brief Returns the number of captured instances.
         * @return Number of immutable render proxies in this submission.
         */
        size_t GetInstanceCount() const { return m_RenderProxies.size(); }

        /**
         * @brief Returns the renderer-facing proxies captured for this frame.
         * @return Immutable render proxies captured for this frame.
         */
        const std::vector<Ref<const SystemInstanceRenderProxy>>& GetRenderProxies() const
        {
            return m_RenderProxies;
        }

    private:
        // Creates a sealed copy that retains only keys accepted by the predicate.
        template <typename T>
        Ref<const FrameSubmission> WithAllowedInstances(T&& isAllowed) const
        {
            auto copy = CreateRef<FrameSubmission>();
            copy->m_RenderProxies = m_RenderProxies;
            copy->m_InstanceKeys = m_InstanceKeys;

            std::erase_if(copy->m_RenderProxies, [&isAllowed](const auto& proxy)
            {
                return !isAllowed(proxy->GetKey());
            });

            std::erase_if(copy->m_InstanceKeys, [&isAllowed](const auto& key)
            {
                return !isAllowed(key);
            });

            copy->m_IsSealed = true;
            return copy;
        }

        // Prevents further instance additions and removals.
        void Seal() { m_IsSealed = true; }

        bool m_IsSealed = false;
        std::vector<Ref<const SystemInstanceRenderProxy>> m_RenderProxies;
        std::unordered_set<SSystemInstanceKey> m_InstanceKeys;
    };

    /**
     * @brief Publishes the latest immutable Aether frame submission.
     *
     * A producer publishes a completed FrameSubmission, and the render path
     * acquires the latest sealed copy. Publishing replaces the previous
     * submission; this class does not queue multiple frames.
     *
     * @thread_safety All public methods synchronize access to the published
     * submission.
     */
    class ELIXIR_API FrameSubmissionPublisher final
    {
    public:
        /**
         * @brief Publishes a submission without filtering instances.
         *
         * @param submission Submission to seal and publish.
         *
         * @pre submission is not null.
         */
        void Publish(Ref<FrameSubmission> submission)
        {
            Publish(std::move(submission), [](const SSystemInstanceKey&)
            {
                return true;
            });
        }

        /**
         * @brief Publishes a filtered copy of a submission.
         *
         * The method seals the result and retains only proxies whose internal keys
         * are accepted by isAllowed.
         *
         * @tparam T Predicate type that accepts SSystemInstanceKey.
         * @param submission Submission to filter, seal, and publish.
         * @param isAllowed Predicate that selects managed instances.
         *
         * @pre submission is not null.
         */
        template <typename T>
        void Publish(Ref<FrameSubmission> submission, T&& isAllowed)
        {
            EE_CORE_ASSERT(submission, "Aether frame submission cannot be null.")
            submission->Seal();

            const auto filtered = submission->WithAllowedInstances(
                std::forward<T>(isAllowed)
            );

            const std::scoped_lock lock(m_Mutex);
            m_Published = std::move(filtered);
        }

        /**
         * @brief Acquires the latest published submission.
         *
         * @return The latest sealed submission, or null when none was published.
         */
        Ref<const FrameSubmission> Acquire()
        {
            const std::scoped_lock lock(m_Mutex);
            return m_Published;
        }

        /**
         * @brief Removes an instance from the currently published submission.
         *
         * Manager calls this method while detaching an instance. The replacement
         * submission remains sealed and does not contain the removed instance.
         *
         * @param instance Runtime instance to remove.
         */
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
