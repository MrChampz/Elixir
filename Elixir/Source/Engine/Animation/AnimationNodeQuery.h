#pragma once

#include <Engine/Animation/AnimationEvaluation.h>

namespace Elixir
{
    /** Queries layers and execution state described by evaluated nodes. */
    class ELIXIR_API AnimationNodeQuery
    {
    public:
        /**
         * @brief Borrow the current node state for layer and playback queries.
         * @param state View valid until the owning evaluator changes.
         */
        explicit AnimationNodeQuery(SAnimationNodeState state);

        /**
         * @brief Find a unique named layer, including qualified child paths.
         * @param name Layer name or slash-separated path.
         */
        AnimationLayer* GetLayer(std::string_view name) const;

        /** @brief Count leaf layers exposed by the node. */
        size_t GetLayerCount() const;

        /** @brief Get the active machine state's name, if exposed. */
        std::string_view GetCurrentState() const;

        /** @brief Check whether a child layer or active state is transitioning. */
        bool IsTransitioning() const;

    private:
        SAnimationNodeState m_State;
    };
}
