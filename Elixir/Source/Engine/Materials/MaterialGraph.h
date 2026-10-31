#pragma once

#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials
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

    /** @brief Maps material parameter names to generated HLSL expressions. */
    struct SMaterialGraphBindings
    {
        std::unordered_map<std::string, std::string> Values;

        std::unordered_map<std::string, std::string> Textures;

        /** Compile-time choices shared by nested function calls. */
        std::unordered_map<std::string, bool> StaticValues;

        /** Arguments of the current function call. */
        std::unordered_map<std::string, SMaterialExpression> FunctionInputs;

        /** Bindless texture array exposed by the selected shader template. */
        std::string TextureArray = "textures";

        /** Default wrapping linear sampler exposed by the template. */
        std::string TextureSampler = "texSampler";
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
         * @brief Checks that nodes and their connections form a valid graph.
         * @param parameters Defines the material parameters available to graph nodes.
         * @param error Receives a description when validation fails.
         * @param bindings Compile-time choices used to check vertex dependencies.
         * @return `true` when the graph can be compiled safely.
         */
        bool Validate(
            const MaterialNodeValidationContext& parameters,
            std::string* error = nullptr,
            const SMaterialGraphBindings* bindings = nullptr
        ) const;

        /** @brief Generates HLSL statements for the configured surface channels. */
        std::string GenerateHLSL() const;

        /**
         * @brief Generates HLSL statements for the configured surface channels.
         * @param bindings Expressions that replace named material parameters.
         * @param stage One shader stage; surface outputs currently support Vertex and Pixel.
         * @return Generated HLSL that uses the supplied parameter bindings.
         */
        std::string GenerateHLSL(
            const SMaterialGraphBindings& bindings,
            EShaderStage stage = EShaderStage::Pixel
        ) const;

        /**
         * @brief Expands one output into an inline expression for a function call.
         * @param nodeId Function output node.
         * @param bindings Parameters and arguments inherited by the call.
         * @param stage One concrete shader stage inherited from the caller.
         */
        SMaterialExpression GenerateExpression(
            uint32_t nodeId,
            const SMaterialGraphBindings& bindings,
            EShaderStage stage = EShaderStage::Pixel
        ) const;

        /**
         * @brief Checks whether a surface channel has an output.
         * @param channel Surface channel to inspect.
         */
        bool HasChannel(const EMaterialChannel channel) const
        {
            return m_Channels.contains(channel);
        }

        /**
         * @brief Return the stages supported by an output and its active dependencies.
         * @param nodeId Output to inspect.
         * @param bindings Compile-time choices used to select active branches.
         */
        EShaderStage GetSupportedStages(
            uint32_t nodeId,
            const SMaterialGraphBindings* bindings = nullptr
        ) const;

      private:
        /**
         * @brief Emit a node and its dependencies.
         * @param id Node to emit.
         * @param emitted Expressions already emitted for this graph evaluation.
         * @param visiting Nodes in the current dependency path.
         * @param body Receives statements for non-inline evaluation.
         * @param bindings Material parameters, static choices, and function arguments.
         * @param stage Concrete shader stage for this emission.
         * @param inlineExpression Expand expressions without local variable declarations.
         */
        SMaterialExpression EmitNode(
            uint32_t id,
            std::unordered_map<uint32_t, SMaterialExpression>& emitted,
            std::unordered_set<uint32_t>& visiting,
            std::string& body,
            const SMaterialGraphBindings* bindings,
            EShaderStage stage,
            bool inlineExpression = false
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
