#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace Elixir
{
    enum class EMaterialIRStage : uint8_t { Pixel, Vertex };

    // Backend-independent operations produced from graph nodes. HLSL spelling and
    // temporary-variable formatting belong to MaterialHLSLEmitter, not to this IR.
    enum class EMaterialIROp : uint8_t
    {
        Constant,
        Scalar,
        Bool,
        GraphParameterScalar,
        GraphParameterColor,
        MaterialParameter,
        TextureParameter,
        TextureObjectParameter,
        TextureObjectSample,
        LegacyTextureSample,
        TexCoord,
        Position,
        Time,
        Sine,
        Panner,
        Checker,
        Noise,
        Multiply,
        Add,
        Subtract,
        Divide,
        Power,
        Dot,
        Lerp,
        OneMinus,
        Saturate,
        Fresnel,
        Custom,
        StaticSwitch,
        ComponentMask,
        AppendVector,
        FunctionFallback,
    };

    struct SMaterialIRValue
    {
        static constexpr uint32_t INLINE = std::numeric_limits<uint32_t>::max();

        uint32_t Instruction = INLINE;
        EGraphValueType Type = EGraphValueType::Float;

        // Compatibility boundary for disconnected DefaultInputs. Authored graph
        // nodes currently store those defaults as HLSL text; regular IR values never
        // use this field and remain backend-independent.
        std::string InlineExpression = "0.0";

        [[nodiscard]] bool IsInstruction() const { return Instruction != INLINE; }
    };

    struct SMaterialIRInstruction
    {
        uint32_t SourceNode = 0;
        EMaterialIROp Op = EMaterialIROp::Constant;
        EGraphValueType Type = EGraphValueType::Float;
        std::vector<SMaterialIRValue> Inputs;
        std::vector<bool> InputConnected;

        // Semantic payload. Only CustomCode and LegacyTextureExpression contain HLSL,
        // preserving compatibility with the two graph features that explicitly author
        // backend expressions today.
        glm::vec4 ConstantValue{ 0.0f };
        std::string Name;
        std::string CustomCode;
        std::string LegacyTextureExpression;
        int32_t ParameterSlot = 0;
        ETextureSampleType TextureSampleType = ETextureSampleType::Color;
        ETextureSampleAddress TextureSampleAddress = ETextureSampleAddress::Wrap;
        ETextureSampleFilter TextureSampleFilter = ETextureSampleFilter::Linear;
        ETextureSampleMipMode TextureSampleMipMode = ETextureSampleMipMode::Auto;
        ETextureSampleOutput TextureSampleOutput = ETextureSampleOutput::RGB;
        uint8_t ComponentMask = 0x1;
    };

    struct SMaterialIROutput
    {
        EMaterialChannel Channel = EMaterialChannel::BaseColor;
        SMaterialIRValue Value;
        size_t AfterInstructionCount = 0;
    };

    struct SMaterialIR
    {
        EMaterialIRStage Stage = EMaterialIRStage::Pixel;
        EMaterialDomain Domain = EMaterialDomain::Surface;
        EMaterialUsage Usages = EMaterialUsage::StaticMesh;
        EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
        float AlphaCutoff = 0.5f;
        std::vector<SMaterialIRInstruction> Instructions;
        std::vector<SMaterialIROutput> Outputs;
    };
}
