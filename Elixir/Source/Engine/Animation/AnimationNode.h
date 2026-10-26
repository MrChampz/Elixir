#pragma once

#include <Engine/Animation/Skeleton.h>
#include <Engine/Animation/AnimationParameters.h>

namespace Elixir
{
    class AnimationNodeEvaluator;

    /** Immutable pose-producing connection within one animation graph. */
    class ELIXIR_API AnimationNode
    {
    public:
        virtual ~AnimationNode();

        /**
         * @brief Validate parameter types and input bindings for this node.
         * @param parameters Character parameter definitions and values.
         * @param bindings Numeric parameter names in source input order.
         */
        virtual bool CanBind(
            const AnimationParameters& parameters,
            std::span<const std::string> bindings
        ) const;

        /** @brief Create independent execution state retained by a playback. */
        virtual Scope<AnimationNodeEvaluator> CreateEvaluator() const = 0;

        /**
         * @brief Check whether this node can animate the supplied skeleton.
         * @param skeleton Skeleton to compare with this node.
         */
        bool IsCompatibleWith(const Ref<const Skeleton>& skeleton) const;

        /** @brief Get the skeleton required by this node. */
        virtual const Ref<const Skeleton>& GetSkeleton() const = 0;

        /** @brief Check whether this immutable definition has a usable skeleton. */
        bool IsValid() const { return bool(GetSkeleton()); }

        /**
         * @brief Validate cyclic options according to the node's own semantics.
         * @param looping Requested cyclic playback option.
         */
        virtual bool IsValidPlayback(bool looping) const { return true; }

        /** @brief Get the number of numeric inputs required by the node. */
        virtual size_t GetInputCount() const { return 0; }
    };
}
