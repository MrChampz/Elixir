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
        Constant,      // a literal float4 value (colour, 0..1)
        Scalar,        // a literal single float
        Vector,        // a literal float3 with free-range components
        ParamScalar,   // a live-editable exposed scalar (GraphParams[slot].x)
        ParamColor,    // a live-editable exposed colour (GraphParams[slot])
        Parameter,     // a named material-instance parameter (mat.<field>)
        TextureSample, // sample a bound texture at a UV (input 0, or the mesh UV)
        TexCoord,      // the mesh UV (input.TexCoord)
        Position,      // world-space position (input.WorldPos)
        Time,          // seconds since start (cbFrame.Time)
        Sine,          // sin(a)
        Panner,        // uv + Time * speed (speed from ConstantValue.xy)
        Checker,       // checkerboard from a UV (scale from ConstantValue.x)
        Noise,         // value noise from a UV (scale from ConstantValue.x)
        Multiply,      // a * b
        Add,           // a + b
        Subtract,      // a - b
        Divide,        // a / b
        Power,         // pow(a, b)
        Dot,           // dot(a, b) -> scalar
        Lerp,          // lerp(a, b, t)
        OneMinus,      // 1 - a
        Saturate,      // saturate(a)
        Fresnel,       // schlick fresnel from N,V
        Custom,        // raw HLSL expression over inputs a, b, c
        FunctionInput, // placeholder for a material-function parameter (editor-only)
        FunctionCall,  // instantiates a saved sub-graph (expanded away before codegen)
    };

    // The surface output a channel drives. Together these form the "master node".
    // WorldPositionOffset is a vertex-stage output (displaces the vertex); the rest
    // are pixel-stage surface outputs.
    enum class EMaterialChannel : uint8_t { BaseColor, Metallic, Roughness, Emissive, Normal, WorldPositionOffset, Opacity };

    // How a graph material composites. Drives both the masked clip in the shader and
    // the pipeline blend/depth state in the renderer.
    enum class EMaterialBlendMode : uint8_t { Opaque, Masked, Translucent, Additive };

    // The lighting model applied to the surface. Injected as a compile-time constant
    // so DXC keeps only the chosen model's code.
    enum class EMaterialShadingModel : uint8_t { DefaultLit, Unlit, Subsurface, ClearCoat, Cloth };

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
        std::string CustomCode;          // Custom -> raw HLSL expression over a, b, c
        int32_t ParamSlot = 0;           // ParamScalar/ParamColor -> GraphParams[slot]
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

        // Blend mode + alpha-test cutoff (Masked). Affects the generated pixel shader
        // (the Masked clip); the renderer reads the mode separately for pipeline state.
        void SetBlend(EMaterialBlendMode mode, float cutoff) { m_BlendMode = mode; m_AlphaCutoff = cutoff; }
        void SetShadingModel(EMaterialShadingModel model) { m_ShadingModel = model; }
        [[nodiscard]] EMaterialShadingModel GetShadingModel() const { return m_ShadingModel; }

        // Generate the HLSL statements for a stage. Pixel stage (default) fills
        // `surface.<channel> = ...;` for the surface channels; vertex stage emits
        // `wpo = ...;` from the WorldPositionOffset channel only.
        [[nodiscard]] std::string GenerateHLSL(bool vertexStage = false) const;

        [[nodiscard]] const std::unordered_map<uint32_t, SMaterialNode>& GetNodes() const { return m_Nodes; }

      private:
        std::string EmitNode(
            uint32_t id,
            bool vertexStage,
            std::unordered_map<uint32_t, std::string>& emitted,
            std::unordered_map<uint32_t, EGraphValueType>& types,
            std::string& body) const;

        std::unordered_map<uint32_t, SMaterialNode> m_Nodes;
        std::unordered_map<uint8_t, uint32_t> m_Channels; // EMaterialChannel -> node id
        uint32_t m_NextId = 1;

        EMaterialBlendMode m_BlendMode = EMaterialBlendMode::Opaque;
        float m_AlphaCutoff = 0.5f;
        EMaterialShadingModel m_ShadingModel = EMaterialShadingModel::DefaultLit;
    };
}
