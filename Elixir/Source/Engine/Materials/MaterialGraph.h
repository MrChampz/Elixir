#pragma once

#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials
{
    namespace Compilation { class MaterialGraphEmitter; }
    using namespace Compilation;

    /**
     * @brief Defines a surface property driven by a material graph.
     */
    enum class EMaterialChannel : uint8_t
    {
        /** @brief Surface base color. */
        BaseColor,

        /** @brief Surface normal. */
        Normal,

        /** @brief Surface metallic value. */
        Metallic,

        /** @brief Surface roughness value. */
        Roughness,

        /** @brief Surface opacity value. */
        Opacity,

        /** @brief Surface emissive color. */
        Emissive,

        /** @brief Color of light transmitted through a subsurface material. */
        SubsurfaceColor,

        /** @brief Color of the fiber sheen on a cloth material. */
        FuzzColor,

        /** @brief Strength of the fiber sheen on a cloth material. */
        Cloth,

        /** @brief Roughness of the fiber sheen on a cloth material. */
        FuzzRoughness,

        /** @brief Ambient-occlusion multiplier for indirect and direct lighting. */
        AmbientOcclusion,

        /** @brief Dielectric specular intensity. */
        Specular,

        /** @brief Dielectric specular tint. */
        SpecularColor,

        /** @brief Coverage of the clear-coat layer. */
        ClearCoat,

        /** @brief Roughness of the clear-coat layer. */
        ClearCoatRoughness,

        /** @brief Tangent-space normal of the layer below clear coat. */
        ClearCoatBottomNormal,

        /** World-space displacement applied before vertex projection. */
        WorldPositionOffset,
    };

    /**
     * @brief Stores a node graph that defines material surface properties.
     *
     * The graph owns node instances and their connections. Each node defines its
     * own inputs, validation rules, and HLSL emission behavior.
     */
    class ELIXIR_API MaterialGraph
    {
        friend class MaterialGraphEmitter;

      public:
        MaterialGraph() = default;

        MaterialGraph(const MaterialGraph&) = delete;
        MaterialGraph& operator=(const MaterialGraph&) = delete;

        MaterialGraph(MaterialGraph&&) noexcept = default;
        MaterialGraph& operator=(MaterialGraph&&) noexcept = default;

        template<typename TNode, typename... TArgs>
        uint32_t AddNode(TArgs&&... args)
        {
            static_assert(std::derived_from<TNode, MaterialNode>);
            return AddNode(CreateScope<TNode>(std::forward<TArgs>(args)...));
        }

        uint32_t AddNode(Scope<MaterialNode> node);

        const MaterialNode* FindNode(uint32_t id) const;

        /**
         * @brief Connects a node output to an input slot.
         * @param fromNode Source node ID.
         * @param toNode Destination node ID.
         * @param toSlot Destination input slot.
         *
         * The call has no effect when the destination node or input slot does not exist.
         */
        void Connect(uint32_t fromNode, uint32_t toNode, uint32_t toSlot);

        /**
         * @brief Connects a node output to a surface channel.
         * @param channel Surface channel to drive.
         * @param nodeId Node that provides the channel value.
         */
        void SetChannel(EMaterialChannel channel, uint32_t nodeId);

        /**
         * @brief Checks whether a surface channel has an output.
         * @param channel Surface channel to inspect.
         */
        bool HasChannel(const EMaterialChannel channel) const
        {
            return m_Channels.contains(channel);
        }

      private:
        struct SGraphNode
        {
            Scope<MaterialNode> Node;
            std::vector<int32_t> Inputs;
        };

        std::unordered_map<uint32_t, SGraphNode> m_Nodes;
        std::unordered_map<EMaterialChannel, uint32_t> m_Channels;
        uint32_t m_NextId = 1;
    };
}
