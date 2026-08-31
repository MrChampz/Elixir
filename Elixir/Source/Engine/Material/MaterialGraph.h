#pragma once

#include <Engine/Material/MaterialNode.h>

namespace Elixir
{
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
    };

    /** @brief Maps material parameter names to generated HLSL expressions. */
    struct SMaterialGraphBindings
    {
        std::unordered_map<std::string, std::string> Values;
        std::unordered_map<std::string, std::string> Textures;
    };

    /**
     * @brief Stores a node graph that defines material surface properties.
     *
     * The graph owns node instances and their connections. Each node defines its
     * own inputs, validation rules, and HLSL emission behavior.
     */
    class ELIXIR_API MaterialGraph
    {
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

        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string* error = nullptr
        ) const;

        /** @brief Generates HLSL statements for the configured surface channels. */
        std::string GenerateHLSL() const;

        /**
         * @brief Generates HLSL statements for the configured surface channels.
         * @param bindings Expressions that replace named material parameters.
         * @return Generated HLSL that uses the supplied parameter bindings.
         */
        std::string GenerateHLSL(const SMaterialGraphBindings& bindings) const;

        const MaterialNode* FindNode(uint32_t id) const;

    private:
        /** @brief Emits HLSL for a node and its dependencies. */
        SMaterialExpression EmitNode(
            uint32_t id,
            std::unordered_map<uint32_t, SMaterialExpression>& emitted,
            std::unordered_set<uint32_t>& visiting,
            std::string& body,
            const SMaterialGraphBindings* bindings
        ) const;

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
