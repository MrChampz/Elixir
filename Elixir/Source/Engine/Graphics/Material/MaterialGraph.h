#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <unordered_map>

namespace Elixir
{
    // The HLSL value type a node output carries.
    enum class EGraphValueType : uint8_t { Float, Float2, Float3, Float4 };

    // The kind of computation a node performs. The codegen switches on this.
    enum class EMaterialNodeType : uint8_t
    {
        Constant,      // a literal value
        Parameter,     // a named material-instance parameter (mat.<field>)
        TextureSample, // sample a bound texture at the mesh UV
        Multiply,      // a * b
        Add,           // a + b
        Lerp,          // lerp(a, b, t)
        Fresnel,       // schlick fresnel from N,V
    };

    // The surface output a channel drives. Together these form the "master node".
    enum class EMaterialChannel : uint8_t { BaseColor, Metallic, Roughness, Emissive, Normal };

    // One node in a material graph. Nodes are plain data (no lambdas) so the graph
    // can be serialized and edited; the codegen interprets Type.
    struct SMaterialNode
    {
        uint32_t Id = 0;
        EMaterialNodeType Type = EMaterialNodeType::Constant;
        EGraphValueType OutputType = EGraphValueType::Float4;

        // For each input slot: the id of the source node, or -1 to use the matching
        // DefaultInputs literal.
        std::vector<int32_t> Inputs;
        std::vector<std::string> DefaultInputs;

        // Per-type payload.
        glm::vec4 ConstantValue{ 0.0f }; // Constant
        std::string ParameterName;       // Parameter -> mat.<ParameterName>
        std::string TextureExpression;   // TextureSample -> the HLSL sample expression
    };

    // A node graph describing a material's surface. Compiles to an HLSL body that
    // fills a surface struct, plugged into a template pixel shader.
    class ELIXIR_API MaterialGraph
    {
      public:
        uint32_t AddNode(const SMaterialNode& node);

        // Wire the output of `fromNode` into input `toSlot` of `toNode`.
        void Connect(uint32_t fromNode, uint32_t toNode, uint32_t toSlot);

        // Drive a surface channel from a node's output.
        void SetChannel(EMaterialChannel channel, uint32_t nodeId);

        // Generate the HLSL statements that fill `surface.<channel> = ...;`.
        [[nodiscard]] std::string GenerateHLSL() const;

        [[nodiscard]] const std::unordered_map<uint32_t, SMaterialNode>& GetNodes() const { return m_Nodes; }

      private:
        std::string EmitNode(uint32_t id, std::unordered_map<uint32_t, std::string>& emitted, std::string& body) const;

        std::unordered_map<uint32_t, SMaterialNode> m_Nodes;
        std::unordered_map<uint8_t, uint32_t> m_Channels; // EMaterialChannel -> node id
        uint32_t m_NextId = 1;
    };
}
