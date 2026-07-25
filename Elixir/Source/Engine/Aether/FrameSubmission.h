#pragma once

#include <unordered_set>
#include <vector>

#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether
{
    // Non-owning list of the system instances selected for one rendering frame.
    // Submitted instances must remain alive and unchanged until Renderer::Render()
    // returns. An instance can be submitted at most once per frame.
    class ELIXIR_API FrameSubmission final
    {
    public:
        bool Submit(const SystemInstance& instance)
        {
            const auto [_, inserted] = m_InstanceIds.insert(instance.GetId());
            if (!inserted) return false;

            m_Instances.push_back(&instance);
            return true;
        }

        void Reset()
        {
            m_Instances.clear();
            m_InstanceIds.clear();
        }

        bool IsEmpty() const { return m_Instances.empty(); }

        size_t GetInstanceCount() const { return m_Instances.size(); }

        const std::vector<const SystemInstance*>& GetInstances() const { return m_Instances; }

    private:
        std::vector<const SystemInstance*> m_Instances;
        std::unordered_set<UUID> m_InstanceIds;
    };
}