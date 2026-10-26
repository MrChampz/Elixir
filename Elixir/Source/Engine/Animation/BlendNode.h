#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationNode.h>
#include <Engine/Animation/AnimationLayer.h>

namespace Elixir
{
    /** Defines one named child node and how its pose contributes to a BlendNode. */
    struct ELIXIR_API SBlendNodeInput
    {
        /** Nonempty name, unique within the blend node; '/' is reserved for layer paths. */
        std::string Name = BaseLayerName;

        /** Null for the base input; later inputs require a compatible bone mask. */
        Ref<const BoneMask> Mask;

        /** Weight in [0, 1], multiplied by mask weights; the base input requires 1. */
        float Weight = 1;

        /** How this pose affects the accumulated pose; the first input requires Override. */
        EAnimationLayerBlendMode BlendMode = EAnimationLayerBlendMode::Override;

        /**
         * Local reference for additive differences; empty uses the playback reference pose.
         * Must be empty for the first input, or contain one valid transform per skeleton bone.
         */
        SLocalPose AdditiveReferencePose;

        /** Retained child node that produces a pose using a compatible skeleton. */
        Ref<const AnimationNode> Node;

        /** Numeric parameter names in child input order, one name per input. */
        std::vector<std::string> InputParameters;

        /** Repeat cyclic playback; the value must satisfy the child node's playback policy. */
        bool Looping = true;

        /**
         * @brief Validate composition and source bindings in their owning configuration.
         * @param skeleton Character skeleton required by every source.
         * @param parameters Typed parameter layout used by the sources.
         * @param first Whether this input provides the underlying full-body pose.
         * @return True when settings are valid and the child can bind the named parameters.
         */
        bool IsValid(
            const Ref<const Skeleton>& skeleton,
            const AnimationParameters& parameters,
            bool first
        ) const;

        /**
         * @brief Validate source, blend settings, and input count without resolving names.
         * @param skeleton Skeleton shared by the connected sources.
         * @param first Whether this input provides the full-body base pose.
         * @return True when this input satisfies its base or masked contribution requirements.
         */
        bool IsValidSettings(const Ref<const Skeleton>& skeleton, bool first) const;
    };

    /**
     * Defines immutable pose composition from a full-body base and ordered masked inputs.
     * Later inputs blend into the accumulated pose in declaration order. Each input has its
     * own weight; weights are not normalized across inputs. Evaluators own playback state.
     */
    class ELIXIR_API BlendNode final : public AnimationNode
    {
    public:
        /**
         * @brief Validate and retain a composition of compatible animation nodes.
         * @param name Nonempty name of this composition node.
         * @param inputs Full-body base followed by masked override or additive inputs.
         * @return Null for invalid composition settings or incompatible skeletons.
         */
        static Ref<BlendNode> Create(
            std::string name,
            std::vector<SBlendNodeInput> inputs
        );

        /** @brief Create independent execution for this composition and its children. */
        Scope<AnimationNodeEvaluator> CreateEvaluator() const override;

        /**
         * @brief Validate named bindings required by all connected sources.
         * @param parameters Shared character parameter layout.
         * @param bindings Must be empty; each input declares its own bindings.
         */
        bool CanBind(
            const AnimationParameters& parameters,
            std::span<const std::string> bindings
        ) const override;

        /**
         * @brief Keep cyclic playback policies on the connected inputs.
         * @param looping Must be true; each input declares its own policy.
         */
        bool IsValidPlayback(const bool looping) const override { return looping; }

        /** @brief Get this node's name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the immutable connected input definitions. */
        const std::vector<SBlendNodeInput>& GetInputs() const { return m_Inputs; }

        /** @brief Get the common skeleton of the connected inputs. */
        const Ref<const Skeleton>& GetSkeleton() const override { return m_Skeleton; }

    private:
        /**
         * @brief Retain a validated composition.
         * @param name Node name.
         * @param inputs Connected inputs in composition order.
         * @param skeleton Common skeleton of the inputs.
         */
        BlendNode(
            std::string name,
            std::vector<SBlendNodeInput> inputs,
            Ref<const Skeleton> skeleton
        );

        std::string m_Name;
        std::vector<SBlendNodeInput> m_Inputs;
        Ref<const Skeleton> m_Skeleton;
    };
}
