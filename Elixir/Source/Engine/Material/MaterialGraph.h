#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Engine/Core/Core.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir
{
    /** @brief Defines a surface property driven by a material graph. */
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
        /** @brief Expressions for numeric material parameters. */
        std::unordered_map<std::string, std::string> Values;
        /** @brief Expressions for texture material parameters. */
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
        /** @brief Creates an empty material graph. */
        MaterialGraph() = default;

        /** @brief Prevents copying node ownership. */
        MaterialGraph(const MaterialGraph&) = delete;

        /** @brief Prevents copying node ownership. */
        MaterialGraph& operator=(const MaterialGraph&) = delete;

        /** @brief Moves node ownership into a new graph. */
        MaterialGraph(MaterialGraph&&) noexcept = default;

        /** @brief Replaces this graph by moving node ownership. */
        MaterialGraph& operator=(MaterialGraph&&) noexcept = default;

        /** @brief Adds a node instance and returns its assigned ID. */
        uint32_t AddNode(std::unique_ptr<IMaterialNode> node);

        /**
         * @brief Constructs and adds a node of the requested type.
         * @tparam TNode Concrete material node type.
         * @param args Arguments forwarded to the node constructor.
         * @return The unique ID assigned to the added node.
         */
        template<typename TNode, typename... TArgs>
        uint32_t AddNode(TArgs&&... args)
        {
            static_assert(std::derived_from<TNode, IMaterialNode>);
            return AddNode(std::make_unique<TNode>(std::forward<TArgs>(args)...));
        }

        /** @brief Connects a source node output to a destination input slot. */
        void Connect(uint32_t fromNode, uint32_t toNode, uint32_t toSlot);

        /** @brief Connects a node output to a surface channel. */
        void SetChannel(EMaterialChannel channel, uint32_t nodeId);

        /** @brief Validates all nodes against material parameters. */
        bool Validate(const IMaterialNodeValidationContext& parameters, std::string* error = nullptr) const;

        /** @brief Generates HLSL using default material parameter expressions. */
        std::string GenerateHLSL() const;

        /** @brief Generates HLSL using supplied material parameter expressions. */
        std::string GenerateHLSL(const SMaterialGraphBindings& bindings) const;

        /** @brief Gets the node identified by an ID, or `nullptr` when it is absent. */
        const IMaterialNode* FindNode(uint32_t id) const;

    private:
        struct SGraphNode
        {
            std::unique_ptr<IMaterialNode> Node;
            std::vector<int32_t> Inputs;
        };

        /** @brief Emits HLSL for a node and its dependencies. */
        SMaterialExpression EmitNode(
            uint32_t id,
            std::unordered_map<uint32_t, SMaterialExpression>& emitted,
            std::unordered_set<uint32_t>& visiting,
            std::string& body,
            const SMaterialGraphBindings* bindings
        ) const;

        std::unordered_map<uint32_t, SGraphNode> m_Nodes;
        std::unordered_map<EMaterialChannel, uint32_t> m_Channels;
        uint32_t m_NextId = 1;
    };
}
