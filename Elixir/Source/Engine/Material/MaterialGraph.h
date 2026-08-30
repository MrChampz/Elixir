#pragma once

namespace Elixir
{
    /**
     * @brief Defines the HLSL value type produced by a graph node.
     */
    enum class EMaterialGraphValueType : uint8_t
    {
        /** @brief One floating-point component. */
        Float,

        /** @brief Two floating-point components. */
        Float2,

        /** @brief Three floating-point components. */
        Float3,

        /** @brief Four floating-point components. */
        Float4,
    };

    /**
     * @brief Defines the operation performed by a material graph node.
     */
    enum class EMaterialNodeType : uint8_t
    {
        /** @brief Outputs a literal value. */
        Constant,

        /** @brief Reads a named material value parameter. */
        Parameter,      // mat.<field>

        /** @brief Outputs the input texture coordinates. */
        TexCoord,       // input.TexCoord

        /** @brief Samples a named material texture parameter. */
        TextureSample,  // sample a bound texture at a UV (input 0)

        /** @brief Selects one component from an input value. */
        ComponentMask,

        /** @brief Outputs elapsed time in seconds. */
        Time,           // cbFrame.Time

        /** @brief Applies the sine function to an input. */
        Sine,           // sin(a)

        /** @brief Offsets texture coordinates over time. */
        Panner,         // uv + Time * speed (speed from ConstantValue.xy)

        /** @brief Generates a procedural checkerboard. */
        Checkerboard,

        /** @brief Generates an exponential radial gradient. */
        RadialGradientExponential,  // pow(saturate(1 - distance / radius), exponent)

        /** @brief Multiplies two input values. */
        Multiply,       // a * b

        /** @brief Adds two input values. */
        Add,            // a + b

        /** @brief Subtracts two input values. */
        Subtract,       // a - b

        /** @brief Divides two input values. */
        Divide,         // a / b

        /** @brief Raises one input value to another. */
        Power,          // pow(a, b)

        /** @brief Calculates the dot product of two input values. */
        Dot,            // dot(a, b) -> scalar

        /** @brief Linearly interpolates between two input values. */
        Lerp,           // lerp(a, b, t)

        /** @brief Subtracts an input value from one. */
        OneMinus,       // 1 - a

        /** @brief Clamps an input value to the zero-to-one range. */
        Saturate,       // saturate(a)

        /** @brief Calculates a Schlick Fresnel factor. */
        Fresnel,
    };

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

    /**
     * @brief Maps material parameter names to generated HLSL expressions.
     */
    struct SMaterialGraphBindings
    {
        /** @brief Expressions for numeric material parameters. */
        std::unordered_map<std::string, std::string> Values;

        /** @brief Expressions for texture material parameters. */
        std::unordered_map<std::string, std::string> Textures;
    };

    /**
     * @brief Stores the data and input connections for one material graph node.
     */
    struct SMaterialNode
    {
        /** @brief Unique graph identifier assigned when the node is added. */
        uint32_t Id = 0;

        /** @brief Operation performed by the node. */
        EMaterialNodeType Type = EMaterialNodeType::Constant;

        /** @brief Value type produced by the node. */
        EMaterialGraphValueType OutputType = EMaterialGraphValueType::Float4;

        /**
         * @brief Source node IDs for each input slot.
         *
         * A value of -1 selects the matching entry in DefaultInputs.
         */
        std::vector<int32_t> Inputs;

        /** @brief Literal expressions used by unconnected input slots. */
        std::vector<std::string> DefaultInputs;

        /** @brief Constant value and type-specific numeric settings. */
        glm::vec4 ConstantValue{ 0.0f };

        /** @brief Material value parameter read by a Parameter node. */
        std::string ParameterName;

        /** @brief Material texture parameter sampled by a TextureSample node. */
        std::string TextureParameterName;

        /** @brief Component selected by a ComponentMask node. */
        uint32_t ComponentIndex = 0;

        /** @brief Center used by a RadialGradientExponential node. */
        glm::vec2 RadialGradientCenter{ 0.5f };

        /** @brief Radius used by a RadialGradientExponential node. */
        float RadialGradientRadius = 0.5f;

        /** @brief Exponent used by a RadialGradientExponential node. */
        float RadialGradientExponent = 1.0f;
    };

    /**
     * @brief Stores a node graph that defines material surface properties.
     *
     * The graph generates an HLSL body that writes values to a material surface
     * structure in a shader template.
     */
    class ELIXIR_API MaterialGraph
    {
    public:
        /**
         * @brief Adds a node to the graph.
         * @param node Node data to add.
         * @return The unique ID assigned to the added node.
         *
         * The graph assigns Id and does not use the ID supplied in node.
         */
        uint32_t AddNode(const SMaterialNode& node);

        /**
         * @brief Connects a node output to an input slot.
         * @param fromNode Source node ID.
         * @param toNode Destination node ID.
         * @param toSlot Destination input slot.
         *
         * The call has no effect when toNode does not exist.
         */
        void Connect(uint32_t fromNode, uint32_t toNode, uint32_t toSlot);

        /**
         * @brief Connects a node output to a surface channel.
         * @param channel Surface channel to drive.
         * @param nodeId Node that provides the channel value.
         */
        void SetChannel(EMaterialChannel channel, uint32_t nodeId);

        /**
         * @brief Generates HLSL statements for the configured surface channels.
         * @return Generated HLSL that uses default material parameter expressions.
         */
        std::string GenerateHLSL() const;

        /**
         * @brief Generates HLSL statements for the configured surface channels.
         * @param bindings Expressions that replace named material parameters.
         * @return Generated HLSL that uses the supplied parameter bindings.
         */
        std::string GenerateHLSL(const SMaterialGraphBindings& bindings) const;

        /** @brief Gets the graph nodes indexed by their assigned IDs. */
        const std::unordered_map<uint32_t, SMaterialNode>& GetNodes() const { return m_Nodes; }

    private:
        /** @brief Emits HLSL for a node and its dependencies. */
        std::string EmitNode(
            uint32_t id,
            std::unordered_map<uint32_t, std::string>& emitted,
            std::unordered_map<uint32_t, EMaterialGraphValueType>& types,
            std::string& body,
            const SMaterialGraphBindings* bindings
        ) const;

        std::unordered_map<uint32_t, SMaterialNode> m_Nodes;
        std::unordered_map<EMaterialChannel, uint32_t> m_Channels;
        uint32_t m_NextId = 1;
    };
}
