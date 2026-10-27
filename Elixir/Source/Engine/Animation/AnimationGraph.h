#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/BlendNode.h>

namespace Elixir
{
    /** Defines parameters and the output node of an immutable animation graph. */
    struct SAnimationGraphCreateInfo
    {
        std::string Name;
        std::vector<SAnimationParameter> Parameters;
        Ref<const AnimationNode> Output;

        /** Numeric parameter names required by the output node, in input order. */
        std::vector<std::string> InputParameters;

        /** Cyclic policy of the output node; composed nodes own their child policies. */
        bool Looping = true;
    };

    /** Shared composition graph that creates independent animation artifacts. */
    class ELIXIR_API AnimationGraph final
    {
    public:
        /**
         * @brief Validate a graph and retain its connected output node.
         * @param info Graph name, parameter defaults, and output connection.
         * @return Null for invalid parameters, output, or node bindings.
         */
        static Ref<AnimationGraph> Create(SAnimationGraphCreateInfo info);

        /**
         * @brief Create a ready animation without evaluating transitions or advancing clocks.
         * @param referencePose Sampling defaults copied for each skeleton bone.
         * @return An independent animation, or null for an invalid reference pose or setup.
         */
        Ref<Animation> CreateAnimation(const SLocalPose& referencePose) const;

        /** @brief Get the skeleton of the output node. */
        const Ref<const Skeleton>& GetSkeleton() const;

        /** @brief Get this graph's name. */
        const std::string& GetName() const { return m_Info.Name; }

        /** @brief Get immutable parameter defaults. */
        const std::vector<SAnimationParameter>& GetParameters() const
        {
            return m_Info.Parameters;
        }

        /** @brief Get the node connected to the graph output. */
        const Ref<const AnimationNode>& GetOutput() const { return m_Info.Output; }

    private:
        /**
         * @brief Retain a validated graph and its bound output adapter.
         * @param info Immutable authored graph.
         * @param root Composition that binds the output to the graph's parameters.
         */
        AnimationGraph(SAnimationGraphCreateInfo info, Ref<const BlendNode> root);

        SAnimationGraphCreateInfo m_Info;
        Ref<const BlendNode> m_Root;
    };
}
