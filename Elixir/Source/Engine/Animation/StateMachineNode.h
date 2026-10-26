#pragma once

#include <Engine/Animation/AnimationNode.h>
#include <Engine/Animation/StateMachine.h>

namespace Elixir
{
    /** Connects a retained StateMachine resource to an animation graph. */
    class ELIXIR_API StateMachineNode final : public AnimationNode
    {
    public:
        /**
         * @brief Create a node that retains an immutable animation resource.
         * @param resource Resource sampled or selected by this node.
         * @return Null if the resource or its skeleton is invalid.
         */
        static Ref<StateMachineNode> Create(Ref<const StateMachine> resource);

        /** @brief Create independent playback for this node connection. */
        Scope<AnimationEvaluator> CreateEvaluator() const override;

        /**
         * @brief Validate the retained machine's conditions and child node bindings.
         * @param parameters Typed character parameter layout.
         * @param bindings Must be empty; states declare their own bindings.
         */
        bool CanBind(
            const AnimationParameters& parameters,
            std::span<const std::string> bindings
        ) const override;

        /**
         * @brief Keep looping policy on the machine's states.
         * @param looping Must be true; individual states declare their own policies.
         */
        bool IsValidPlayback(const bool looping) const override { return looping; }

        /** @brief Inspect the immutable resource referenced by this node. */
        const Ref<const StateMachine>& GetStateMachine() const { return m_Resource; }

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
        explicit StateMachineNode(Ref<const StateMachine> resource);

        Ref<const StateMachine> m_Resource;
    };
}
