#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Elixir
{
    class MaterialGraphLegacyTestAccess;

    // The HLSL value type a node output carries.
    enum class EGraphValueType : uint8_t { Float, Float2, Float3, Float4, Texture2D };

    enum class ETextureSampleType : uint8_t { Color, Linear, Normal, Masks };
    enum class ETextureSampleAddress : uint8_t { Wrap, Clamp };
    enum class ETextureSampleFilter : uint8_t { Linear, Point };
    enum class ETextureSampleMipMode : uint8_t { Auto, Level, Bias };
    enum class ETextureSampleOutput : uint8_t { RGB, R, G, B, A, RGBA };

    // The kind of computation a node performs. The codegen switches on this.
    enum class EMaterialNodeType : uint8_t
    {
        Constant,      // a literal float4 value (colour, 0..1)
        Scalar,        // a literal single float
        Vector,        // a literal float3 with free-range components
        Bool,          // a 0/1 toggle (scalar)
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
        StaticBoolParameter, // named compile-time bool overridden by a material instance
        StaticSwitch,  // compile-time choice: true input, false input, static-bool condition
        TextureParameter, // instance-overridable bindless Texture2D sample
        TextureObjectParameter, // instance-overridable bindless Texture2D handle
        TextureObjectSample, // sample input 0 (Texture2D) at input 1 (UV)
        ComponentMask, // select an ordered subset of RGBA from input 0
        AppendVector, // concatenate the components of inputs 0 and 1
    };

    // The surface output a channel drives. Together these form the "master node".
    // WorldPositionOffset is a vertex-stage output (displaces the vertex); the rest
    // are pixel-stage surface outputs.
    enum class EMaterialChannel : uint8_t
    {
        BaseColor, Metallic, Roughness, Emissive, Normal, WorldPositionOffset, Opacity,
        SubsurfaceColor, ClearCoat, ClearCoatRoughness, Sheen // shading-model-specific
    };

    // How a graph material composites. Drives both the masked clip in the shader and
    // the pipeline blend/depth state in the renderer.
    enum class EMaterialBlendMode : uint8_t { Opaque, Masked, Translucent, Additive };

    // The lighting model applied to the surface. Injected as a compile-time constant
    // so DXC keeps only the chosen model's code.
    enum class EMaterialShadingModel : uint8_t { DefaultLit, Unlit, Subsurface, ClearCoat, Cloth };

    enum class EMaterialDiagnosticSeverity : uint8_t { Warning, Error };

    // A validation message tied to the editor node that caused it. NodeId == 0
    // identifies a graph/output-level diagnostic rather than a regular node.
    struct SMaterialGraphDiagnostic
    {
        EMaterialDiagnosticSeverity Severity = EMaterialDiagnosticSeverity::Error;
        uint32_t NodeId = 0;
        std::string Message;
    };

    struct SMaterialGraphValidation
    {
        std::vector<SMaterialGraphDiagnostic> Diagnostics;

        [[nodiscard]] bool HasErrors() const
        {
            for (const auto& diagnostic : Diagnostics)
                if (diagnostic.Severity == EMaterialDiagnosticSeverity::Error)
                    return true;
            return false;
        }
    };

    // One node in a material graph. Nodes are plain data (no lambdas) so the graph
    // can be serialized and edited; the codegen interprets Type.
    struct SMaterialNode
    {
        uint32_t Id = 0;
        // Stable editor node id used for diagnostics. Expanded function nodes inherit
        // the id of their visible FunctionCall node.
        uint32_t SourceId = 0;
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
        int32_t ParamSlot = 0;           // exposed runtime parameter -> GraphParams[slot]
        ETextureSampleType TextureSampleType = ETextureSampleType::Color;
        ETextureSampleAddress TextureSampleAddress = ETextureSampleAddress::Wrap;
        ETextureSampleFilter TextureSampleFilter = ETextureSampleFilter::Linear;
        ETextureSampleMipMode TextureSampleMipMode = ETextureSampleMipMode::Auto;
        ETextureSampleOutput TextureSampleOutput = ETextureSampleOutput::RGB;
        uint8_t ComponentMask = 0x1; // R=1, G=2, B=4, A=8
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
        [[nodiscard]] EMaterialBlendMode GetBlendMode() const { return m_BlendMode; }
        [[nodiscard]] float GetAlphaCutoff() const { return m_AlphaCutoff; }
        [[nodiscard]] EMaterialShadingModel GetShadingModel() const { return m_ShadingModel; }

        // Generate the HLSL statements for a stage. Pixel stage (default) fills
        // `surface.<channel> = ...;` for the surface channels; vertex stage emits
        // `wpo = ...;` from the WorldPositionOffset channel only.
        [[nodiscard]] std::string GenerateHLSL(bool vertexStage = false) const;

        // Validate the reachable graph before code generation/compilation. Reports
        // malformed links, cycles, unsupported stage usage and invalid node payloads.
        [[nodiscard]] SMaterialGraphValidation Validate() const;

        [[nodiscard]] const std::unordered_map<uint32_t, SMaterialNode>& GetNodes() const { return m_Nodes; }
        [[nodiscard]] const std::unordered_map<uint8_t, uint32_t>& GetChannels() const { return m_Channels; }

      private:
        friend class MaterialGraphLegacyTestAccess;

        // Transitional oracle used only by MaterialIR equivalence tests. Runtime
        // generation goes through MaterialIRBuilder + MaterialHLSLEmitter.
        [[nodiscard]] std::string GenerateLegacyHLSL(bool vertexStage) const;
        std::string EmitLegacyNode(
            uint32_t id,
            bool vertexStage,
            std::unordered_map<uint32_t, std::string>& emitted,
            std::unordered_map<uint32_t, EGraphValueType>& types,
            std::unordered_set<uint32_t>& visiting,
            std::string& body) const;

        std::unordered_map<uint32_t, SMaterialNode> m_Nodes;
        std::unordered_map<uint8_t, uint32_t> m_Channels; // EMaterialChannel -> node id
        uint32_t m_NextId = 1;

        EMaterialBlendMode m_BlendMode = EMaterialBlendMode::Opaque;
        float m_AlphaCutoff = 0.5f;
        EMaterialShadingModel m_ShadingModel = EMaterialShadingModel::DefaultLit;
    };
}
