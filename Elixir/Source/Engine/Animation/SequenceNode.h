#pragma once

#include <Engine/Animation/AnimationNode.h>
#include <Engine/Animation/Sequence.h>

namespace Elixir
{
    /** Connects a retained Sequence resource to an animation graph. */
    class ELIXIR_API SequenceNode final : public AnimationNode
    {
    public:
        /**
         * @brief Create a node that retains an immutable animation resource.
         * @param resource Resource sampled or selected by this node.
         * @return Null if the resource or its skeleton is invalid.
         */
        static Ref<SequenceNode> Create(Ref<const Sequence> resource);

        /** @brief Create independent playback for this node connection. */
        Scope<AnimationEvaluator> CreateEvaluator() const override;

        /** @brief Inspect the immutable resource referenced by this node. */
        const Ref<const Sequence>& GetSequence() const { return m_Resource; }

        /** @brief Get the skeleton required by the retained resource. */
        const Ref<const Skeleton>& GetSkeleton() const override
        {
            return m_Resource->GetSkeleton();
        }

    private:
        /**
         * @brief Retain a validated resource for the node's lifetime.
         * @param resource Non-null immutable animation resource.
         */
        explicit SequenceNode(Ref<const Sequence> resource);

        Ref<const Sequence> m_Resource;
    };
}
