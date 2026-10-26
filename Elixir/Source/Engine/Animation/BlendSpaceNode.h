#pragma once

#include <Engine/Animation/AnimationNode.h>
#include <Engine/Animation/BlendSpace.h>

namespace Elixir
{
    /** Connects a retained BlendSpace resource to an animation graph. */
    class ELIXIR_API BlendSpaceNode final : public AnimationNode
    {
    public:
        /**
         * @brief Create a node that retains an immutable animation resource.
         * @param resource Resource sampled or selected by this node.
         * @return Null if the resource or its skeleton is invalid.
         */
        static Ref<BlendSpaceNode> Create(Ref<const BlendSpace> resource);

        /** @brief Create independent playback for this node connection. */
        Scope<AnimationNodeEvaluator> CreateEvaluator() const override;

        /** @brief Get the skeleton required by the retained resource. */
        const Ref<const Skeleton>& GetSkeleton() const override;

        /** @brief Get the number of coordinates required by the retained blend space. */
        size_t GetInputCount() const override;

        /** @brief Inspect the immutable resource referenced by this node. */
        const Ref<const BlendSpace>& GetBlendSpace() const { return m_Resource; }

    private:
        /**
         * @brief Retain a validated resource for the node's lifetime.
         * @param resource Non-null immutable animation resource.
         */
        explicit BlendSpaceNode(Ref<const BlendSpace> resource);

        Ref<const BlendSpace> m_Resource;
    };
}
